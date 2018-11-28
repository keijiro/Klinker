#pragma once

#include "Common.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <tuple>

namespace klinker
{
    //
    // Frame sender class
    //
    // There are two modes that determine how output frames are scheduled.
    //
    // * Async mode
    //
    // Output frames are asynchronously scheduled by the completion callback.
    // Unity can update the frame at any time, but it's not guaranteed to be
    // scheduled, as the completion callback only takes the latest state.
    //
    // The length of the output queue is adjusted by prerolling.
    //
    // * Manual mode
    //
    // Output frames are directly scheduled by Unity. It can guarantee that
    // all frames are to be scheduled on time but needs to be explicitly
    // synchronized to output refreshing. The WaitFrameCompletion method is
    // provided for this purpose.
    //
    // The length of the output queue is controlled by Unity.
    //
    class Sender final : private IDeckLinkVideoOutputCallback
    {
    public:

        #pragma region Constructor/destructor

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

        float GetFrameRate() const
        {
            return static_cast<float>(timeScale_) / frameDuration_;
        }

        bool IsProgressive() const
        {
            assert(displayMode_ != nullptr);
            return displayMode_->GetFieldDominance() == bmdProgressiveFrame;
        }

        bool IsReferenceLocked() const
        {
            assert(output_ != nullptr);
            BMDReferenceStatus stat;
            AssertSuccess(output_->GetReferenceStatus(&stat));
            return stat & bmdReferenceLocked;
        }

        #pragma endregion

        #pragma region Public methods

        void StartAsyncMode(int deviceIndex, int formatIndex, int preroll)
        {
            assert(output_ == nullptr);
            assert(displayMode_ == nullptr);
            assert(frame_ == nullptr);

            InitializeOutput(deviceIndex, formatIndex);

            // Prerolling
            frame_ = AllocateFrame();
            for (auto i = 0; i < preroll; i++) ScheduleFrame(frame_);

            AssertSuccess(output_->StartScheduledPlayback(0, 1, 1));
        }

        void StartManualMode(int deviceIndex, int formatIndex)
        {
            assert(output_ == nullptr);
            assert(displayMode_ == nullptr);
            assert(frame_ == nullptr);

            InitializeOutput(deviceIndex, formatIndex);
            AssertSuccess(output_->StartScheduledPlayback(0, 1, 1));
        }

        void Stop()
        {
            assert(output_ != nullptr);
            assert(displayMode_ != nullptr);

            // Stop the output stream.
            output_->StopScheduledPlayback(0, nullptr, 1);
            output_->SetScheduledFrameCompletionCallback(nullptr);
            output_->DisableVideoOutput();

            // Release the internal objects.
            if (frame_ != nullptr)
            {
                frame_->Release();
                frame_ = nullptr;
            }

            displayMode_->Release();
            displayMode_ = nullptr;

            output_->Release();
            output_ = nullptr;
        }

        void FeedFrame(void* frameData)
        {
            assert(output_ != nullptr);
            assert(displayMode_ != nullptr);

            // Allocate a new frame for the fed data.
            auto newFrame = AllocateFrame();
            CopyFrameData(newFrame, frameData);

            if (IsAsyncMode())
            {
                // Async mode: Replace the frame_ object with it.
                std::lock_guard<std::mutex> lock(mutex_);
                frame_->Release();
                frame_ = newFrame;
            }
            else
            {
                // Manual mode: Immediately schedule it.
                ScheduleFrame(newFrame);
                newFrame->Release();

                // Note: We don't have to use the mutex because nothing here
                // conflicts with the completion callback.
            }
        }

        void WaitFrameCompletion(std::uint64_t frameNumber)
        {
            // Wait for completion of a specified frame.
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [=]() { return counters_.completed >= frameNumber; });
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
            #if defined(_DEBUG)

            if (result == bmdOutputFrameDisplayedLate)
                std::printf("Frame %p was displayed late.\n", completedFrame);

            if (result == bmdOutputFrameDropped)
                std::printf("Frame %p was dropped.\n", completedFrame);

            #endif

            std::lock_guard<std::mutex> lock(mutex_);

            // Increment the frame count and notify the main thread.
            counters_.completed++;
            condition_.notify_all();

            // Async mode: Schedule the next frame.
            if (IsAsyncMode()) ScheduleFrame(frame_);

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped() override
        {
            return S_OK;
        }

        #pragma endregion

    private:

        #pragma region Private members

        std::atomic<ULONG> refCount_ = 1;

        IDeckLinkOutput* output_ = nullptr;
        IDeckLinkDisplayMode* displayMode_ = nullptr;
        IDeckLinkMutableVideoFrame* frame_ = nullptr;

        BMDTimeValue frameDuration_ = 0;
        BMDTimeScale timeScale_ = 1;

        std::mutex mutex_;
        std::condition_variable condition_;

        struct
        {
            std::uint64_t queued = 0;
            std::uint64_t completed = 0;
        }
        counters_;

        bool IsAsyncMode() const
        {
            // This is a little bit hackish, but we can determine the
            // current mode by checking if we have a frame_ object.
            return frame_ != nullptr;
        }

        IDeckLinkMutableVideoFrame* AllocateFrame()
        {
            auto width = displayMode_->GetWidth();
            auto height = displayMode_->GetHeight();
            IDeckLinkMutableVideoFrame* frame;
            AssertSuccess(output_->CreateVideoFrame(
                width, height, width * 2,
                bmdFormat8BitYUV, bmdFrameFlagDefault, &frame
            ));
            return frame;
        }

        void CopyFrameData(IDeckLinkMutableVideoFrame* frame, const void* data) const
        {
            auto width = displayMode_->GetWidth();
            auto height = displayMode_->GetHeight();
            void* pointer;
            AssertSuccess(frame->GetBytes(&pointer));
            std::memcpy(pointer, data, (std::size_t)2 * width * height);
        }

        void ScheduleFrame(IDeckLinkMutableVideoFrame* frame)
        {
            auto time = frameDuration_ * counters_.queued++;
            output_->ScheduleVideoFrame(frame, time, frameDuration_, timeScale_);
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

            dmIterator->Release(); // The iterator is no longer needed.

            // Set this object as a frame completion callback.
            AssertSuccess(output_->SetScheduledFrameCompletionCallback(this));

            // Enable the video output.
            AssertSuccess(output_->EnableVideoOutput(
                displayMode_->GetDisplayMode(), bmdVideoOutputFlagDefault
            ));
        }

        #pragma endregion
    };
}
