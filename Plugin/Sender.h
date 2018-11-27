#pragma once

#include "Common.h"
#include <atomic>
#include <condition_variable>
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

        ~Sender()
        {
            // Internal objects should have been released.
            assert(output_ == nullptr);
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
            return static_cast<float>(timeScale_) / frameDuration_;
        }

        bool IsProgressive() const
        {
            assert(displayMode_ != nullptr);
            return displayMode_->GetFieldDominance() == bmdProgressiveFrame;
        }

        bool IsReferenceLocked() const
        {
            BMDReferenceStatus stat;
            AssertSuccess(output_->GetReferenceStatus(&stat));
            return stat & bmdReferenceLocked;
        }

        #pragma endregion

        #pragma region Public methods

        void StartSending(int deviceIndex, int formatIndex)
        {
            assert(output_ == nullptr);
            assert(displayMode_ == nullptr);

            // Initialize a video output.
            InitializeOutput(deviceIndex, formatIndex);

            // Start getting callback from the video output.
            AssertSuccess(output_->SetScheduledFrameCompletionCallback(this));

            // Enable the video output.
            AssertSuccess(output_->EnableVideoOutput(
                displayMode_->GetDisplayMode(), bmdVideoOutputFlagDefault
            ));

            // Start scheduled playback.
            AssertSuccess(output_->StartScheduledPlayback(0, 1, 1));
        }

        void StopSending()
        {
            assert(output_ != nullptr);
            assert(displayMode_ != nullptr);

            // Stop the output stream.
            output_->StopScheduledPlayback(0, nullptr, 1);
            output_->SetScheduledFrameCompletionCallback(nullptr);
            output_->DisableVideoOutput();

            // Release the internal objects.
            displayMode_->Release();
            displayMode_ = nullptr;
            output_->Release();
            output_ = nullptr;
        }

        void EnqueueFrame(void* data)
        {
            assert(output_ != nullptr);
            assert(displayMode_ != nullptr);

            auto width = displayMode_->GetWidth();
            auto height = displayMode_->GetHeight();

            // Allocate a new frame.
            IDeckLinkMutableVideoFrame* frame;
            AssertSuccess(output_->CreateVideoFrame(
                width, height, width * 2, bmdFormat8BitYUV, bmdFrameFlagDefault, &frame
            ));

            // Copy the frame data.
            void* pointer;
            AssertSuccess(frame->GetBytes(&pointer));
            std::memcpy(pointer, data, (std::size_t)2 * width * height);

            // Add the frame to the queue.
            auto time = frameDuration_ * (counters_.queued + counters_.skipped);
            output_->ScheduleVideoFrame(frame, time, frameDuration_, timeScale_);
            counters_.queued++;

            // Move the ownership of the frame to the scheduler.
            frame->Release();
        }

        void WaitCompletion(std::uint64_t frame)
        {
            // Wait for completion of a specified frame.
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [=]() { return counters_.completed >= frame; });
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
            {
                // The next frame should be skipped to compensate the delay.
                counters_.skipped++;
                std::printf("Frame %p was displayed late.\n", completedFrame);
            }

            if (result == bmdOutputFrameDropped)
            {
                std::printf("Frame %p was dropped.\n", completedFrame);
            }

            // Increment the frame count and notify the main thread.
            std::lock_guard<std::mutex> lock(mutex_);
            counters_.completed++;
            condition_.notify_all();

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

        BMDTimeValue frameDuration_ = 0;
        BMDTimeScale timeScale_ = 1;

        std::mutex mutex_;
        std::condition_variable condition_;

        struct
        {
            std::uint64_t queued = 0;
            std::uint64_t completed = 0;
            std::uint64_t skipped = 0;
        }
        counters_;

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
