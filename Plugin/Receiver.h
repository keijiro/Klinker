#pragma once

#include "Common.h"
#include <atomic>
#include <mutex>
#include <queue>
#include <tuple>
#include <vector>

namespace klinker
{
    //
    // Frame receiver class
    //
    // Arrived frames will be stored in an internal queue that is only used to
    // avoid frame dropping. Frame rate matching should be done on the
    // application side.
    //
    class Receiver final : private IDeckLinkInputCallback
    {
    public:

        #pragma region Constructor/destructor

        ~Receiver()
        {
            // Internal objects should have been released.
            assert(input_ == nullptr);
            assert(displayMode_ == nullptr);
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
            BMDTimeValue duration;
            BMDTimeScale scale;
            AssertSuccess(displayMode_->GetFrameRate(&duration, &scale));
            return static_cast<float>(scale) / duration;
        }

        bool IsProgressive() const
        {
            assert(displayMode_ != nullptr);
            return displayMode_->GetFieldDominance() == bmdProgressiveFrame;
        }

        std::size_t CalculateFrameDataSize() const
        {
            assert(displayMode_ != nullptr);
            return (std::size_t)2 *
                displayMode_->GetWidth() * displayMode_->GetHeight();
        }

        BSTR RetrieveFormatName() const
        {
            assert(displayMode_ != nullptr);
            std::lock_guard<std::mutex> lock(mutex_);
            BSTR name;
            AssertSuccess(displayMode_->GetName(&name));
            return name;
        }

        #pragma endregion

        #pragma region Frame queue methods

        std::size_t CountQueuedFrames() const
        {
            return frameQueue_.size();
        }

        void DequeueFrame()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            frameQueue_.pop();
        }

        const uint8_t* LockOldestFrameData()
        {
            mutex_.lock();
            return frameQueue_.front().data();
        }

        void UnlockOldestFrameData()
        {
            mutex_.unlock();
        }

        #pragma endregion

        #pragma region Public methods

        void Start(int deviceIndex, int formatIndex)
        {
            assert(input_ == nullptr);
            assert(displayMode_ == nullptr);

            InitializeInput(deviceIndex, formatIndex);
            AssertSuccess(input_->StartStreams());
        }

        void Stop()
        {
            assert(input_ != nullptr);
            assert(displayMode_ != nullptr);

            // Stop the input stream.
            input_->StopStreams();
            input_->SetCallback(nullptr);
            input_->DisableVideoInput();

            // Release the internal objects.
            displayMode_->Release();
            displayMode_ = nullptr;

            input_->Release();
            input_ = nullptr;
        }

        #pragma endregion

        #pragma region IUnknown implementation

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override
        {
            if (iid == IID_IUnknown)
            {
                *ppv = this;
                return S_OK;
            }

            if (iid == IID_IDeckLinkInputCallback)
            {
                *ppv = (IDeckLinkInputCallback*)this;
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

        #pragma region IDeckLinkInputCallback implementation

        HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
            BMDVideoInputFormatChangedEvents events,
            IDeckLinkDisplayMode* mode,
            BMDDetectedVideoInputFormatFlags flags
        ) override
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);

                // Update the display mode information.
                displayMode_->Release();
                displayMode_ = mode;
                mode->AddRef();

                // Flush the frame queue.
                while (!frameQueue_.empty()) frameQueue_.pop();
            }

            // Change the video input format as notified.
            input_->PauseStreams();
            input_->EnableVideoInput(
                displayMode_->GetDisplayMode(),
                bmdFormat8BitYUV,
                bmdVideoInputEnableFormatDetection
            );
            input_->FlushStreams();
            input_->StartStreams();

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(
            IDeckLinkVideoInputFrame* videoFrame,
            IDeckLinkAudioInputPacket* audioPacket
        ) override
        {
            if (videoFrame == nullptr) return S_OK;

            if (frameQueue_.size() >= maxQueueLength_)
            {
                std::printf("Overqueued: Arrived frame %p was dropped.", videoFrame);
                return S_OK;
            }

            // Calculate the data size.
            auto size = videoFrame->GetRowBytes() * videoFrame->GetHeight();
            assert(size == CalculateFrameDataSize());

            // Retrieve the data pointer.
            std::uint8_t* source;
            AssertSuccess(videoFrame->GetBytes(reinterpret_cast<void**>(&source)));

            // Allocate and push a new frame to the frame queue.
            std::lock_guard<std::mutex> lock(mutex_);
            frameQueue_.emplace(source, source + size);

            return S_OK;
        }

        #pragma endregion

    private:

        #pragma region Private members

        std::atomic<ULONG> refCount_ = 1;

        IDeckLinkInput* input_ = nullptr;
        IDeckLinkDisplayMode* displayMode_ = nullptr;

        std::queue<std::vector<uint8_t>> frameQueue_;
        mutable std::mutex mutex_;

        static const std::size_t maxQueueLength_ = 5;

        void InitializeInput(int deviceIndex, int formatIndex)
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
                IID_IDeckLinkInput, reinterpret_cast<void**>(&input_)
            ));

            device->Release(); // The device object is no longer needed.

            // Display mode iterator
            IDeckLinkDisplayModeIterator* dmIterator;
            AssertSuccess(input_->GetDisplayModeIterator(&dmIterator));

            // Iterate until reaching the specified index.
            for (auto i = 0; i <= formatIndex; i++)
            {
                if (displayMode_ != nullptr) displayMode_->Release();
                AssertSuccess(dmIterator->Next(&displayMode_));
            }

            dmIterator->Release(); // The iterator is no longer needed.

            // Set this object as a frame input callback.
            AssertSuccess(input_->SetCallback(this));

            // Enable the video input.
            AssertSuccess(input_->EnableVideoInput(
                displayMode_->GetDisplayMode(),
                bmdFormat8BitYUV,
                bmdVideoInputEnableFormatDetection
            ));
        }

        #pragma endregion
    };
}
