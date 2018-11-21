#pragma once

#include "Common.h"
#include <atomic>
#include <mutex>
#include <tuple>

namespace klinker
{
    // Frame receiver class
    // It also privately implements a video output completion callback.
    class Sender final : private IDeckLinkVideoOutputCallback
    {
    public:

        #pragma region Constructor/destructor

        Sender()
            : refCount_(1),
              output_(nullptr), displayMode_(nullptr), frame_(nullptr),
              frameDuration_(0), timeScale_(1), frameCount_(0)
        {
        }

        ~Sender()
        {
            // Internal objects should have been released.
            assert(output_ == nullptr);
            assert(displayMode_ == nullptr);
            assert(frame_ == nullptr);
        }

        #pragma endregion

        #pragma region Accessor methods

        std::tuple<int, int> GetFrameDimensions() const
        {
            assert(displayMode_ != nullptr);
            return { displayMode_->GetWidth(), displayMode_->GetHeight() };
        }

        bool IsReferenceLocked() const
        {
            BMDReferenceStatus stat;
            AssertSuccess(output_->GetReferenceStatus(&stat));
            return stat & bmdReferenceLocked;
        }

        #pragma endregion

        #pragma region Public methods

        void StartSending(int deviceIndex, int formatIndex, int preroll)
        {
            assert(output_ == nullptr);
            assert(displayMode_ == nullptr);
            assert(frame_ == nullptr);

            InitializeOutput(deviceIndex, formatIndex);

            // Create a working frame buffer.
            frame_ = AllocateFrame();

            // Start getting callback from the output object.
            AssertSuccess(output_->SetScheduledFrameCompletionCallback(this));

            // Enable video output.
            AssertSuccess(output_->EnableVideoOutput(
                displayMode_->GetDisplayMode(), bmdVideoOutputFlagDefault
            ));

            // Prerolling with blank frames.
            for (auto i = 0; i < preroll; i++) ScheduleFrame(frame_);

            // Start scheduled playback.
            AssertSuccess(output_->StartScheduledPlayback(0, 1, 1));
        }

        void StopSending()
        {
            assert(output_ != nullptr);
            assert(displayMode_ != nullptr);
            assert(frame_ != nullptr);

            // Stop the output stream.
            output_->StopScheduledPlayback(0, nullptr, 1);
            output_->SetScheduledFrameCompletionCallback(nullptr);
            output_->DisableVideoOutput();

            // Release the internal objects.
            frame_->Release();
            frame_ = nullptr;
            displayMode_->Release();
            displayMode_ = nullptr;
            output_->Release();
            output_ = nullptr;
        }

        void UpdateFrame(void* data)
        {
            assert(output_ != nullptr);
            assert(displayMode_ != nullptr);
            assert(frame_ != nullptr);

            // Allocate a new frame.
            IDeckLinkMutableVideoFrame* newFrame = AllocateFrame();

            // Copy the frame data.
            void* pointer;
            AssertSuccess(newFrame->GetBytes(&pointer));
            std::size_t size = (std::size_t)2 *
                displayMode_->GetWidth() * displayMode_->GetHeight();
            std::memcpy(pointer, data, size);

            // Release and replace the previous frame object with the new one.
            std::lock_guard<std::mutex> lock(frameMutex_);
            frame_->Release();
            frame_ = newFrame;
        }

        #pragma endregion

        #pragma region IUnknown implementation

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override
        {
            if (iid == IID_IUnknown)
            {
                *ppv = this;
                AddRef();
                return S_OK;
            }

            if (iid == IID_IDeckLinkVideoOutputCallback)
            {
                *ppv = (IDeckLinkVideoOutputCallback*)this;
                AddRef();
                return S_OK;
            }

            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return refCount_.fetch_add(1);
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            auto val = refCount_.fetch_sub(1);
            if (val == 1) delete this;
            return val;
        }
        
        #pragma endregion

        #pragma region IDeckLinkVideoOutputCallback implementation

        HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(
            IDeckLinkVideoFrame *completedFrame,
            BMDOutputFrameCompletionResult result
        ) override
        {
            if (result == bmdOutputFrameDisplayedLate)
                std::printf("Frame %p was displayed late.\n", completedFrame);

            if (result == bmdOutputFrameDropped)
                std::printf("Frame %p was dropped.\n", completedFrame);

            // Skip a single frame when DisplayedLate was detected.
            if (result == bmdOutputFrameDisplayedLate) frameCount_++;

            std::lock_guard<std::mutex> lock(frameMutex_);
            ScheduleFrame(frame_);

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped() override
        {
            return S_OK;
        }

        #pragma endregion

    private:

        #pragma region Private members

        std::atomic<ULONG> refCount_;

        IDeckLinkOutput* output_;
        IDeckLinkDisplayMode* displayMode_;

        IDeckLinkMutableVideoFrame* frame_;
        std::mutex frameMutex_;

        BMDTimeValue frameDuration_;
        BMDTimeScale timeScale_;
        uint64_t frameCount_;

        IDeckLinkMutableVideoFrame* AllocateFrame()
        {
            IDeckLinkMutableVideoFrame* frame;
            AssertSuccess(output_->CreateVideoFrame(
                displayMode_->GetWidth(), displayMode_->GetHeight(),
                displayMode_->GetWidth() * 2,
                bmdFormat8BitYUV, bmdFrameFlagDefault, &frame
            ));
            return frame;
        }

        void ScheduleFrame(IDeckLinkVideoFrame* frame)
        {
            output_->ScheduleVideoFrame(
                frame, frameDuration_ * frameCount_++,
                frameDuration_, timeScale_
            );
        }

        void InitializeOutput(int deviceIndex, int formatIndex)
        {
            // Device iterator
            IDeckLinkIterator* iterator;
            AssertSuccess(CoCreateInstance(
                CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL,
                IID_IDeckLinkIterator, reinterpret_cast<void**>(&iterator)
            ));

            // Iterate until reaching the specified index.
            IDeckLink* device = nullptr;
            for (auto i = 0; i <= deviceIndex; i++)
            {
                if (device != nullptr) device->Release();
                AssertSuccess(iterator->Next(&device));
            }

            iterator->Release(); // The iterator is no longer needed.

            // Output interface of the specified device
            AssertSuccess(device->QueryInterface(
                IID_IDeckLinkOutput, reinterpret_cast<void**>(&output_)
            ));

            device->Release(); // The device object is no longer needed.

            // Display mode iterator
            IDeckLinkDisplayModeIterator* dmIterator;
            AssertSuccess(output_->GetDisplayModeIterator(&dmIterator));

            // Iterate until reaching the specified index.
            for (auto i = 0; i <= formatIndex; i++)
            {
                if (displayMode_ != nullptr) displayMode_->Release();
                AssertSuccess(dmIterator->Next(&displayMode_));
            }

            // Get the frame rate defined in the display mode.
            AssertSuccess(displayMode_->GetFrameRate(&frameDuration_, &timeScale_));

            // Cleaning up
            dmIterator->Release();
        }

        #pragma endregion
    };
}
