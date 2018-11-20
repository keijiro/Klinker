#pragma once

#include "Common.h"
#include <atomic>

namespace klinker
{
    class Sender final : public IDeckLinkVideoOutputCallback
    {
    public:

        // Constructor/destructor

        Sender()
            : refCount_(1), output_(nullptr), frame_(nullptr), frameCount_(0)
        {
        }

        ~Sender()
        {
            // Internal objects should have been released;
            assert(output_ == nullptr);
            assert(frame_ == nullptr);
        }

        // Public methods

        void StartSending()
        {
            assert(output_ == nullptr);
            assert(frame_ == nullptr);

            // Retrieve the first DeckLink device using an iterator.
            IDeckLinkIterator* iterator;
            AssertSuccess(CoCreateInstance(
                CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL,
                IID_IDeckLinkIterator, reinterpret_cast<void**>(&iterator)
            ));

            IDeckLink* device;
            AssertSuccess(iterator->Next(&device));

            // Retrieve an output interface from the device.
            AssertSuccess(device->QueryInterface(
                IID_IDeckLinkOutput, reinterpret_cast<void**>(&output_)
            ));

            // These references are no longer needed.
            iterator->Release();
            device->Release();

            // Create a working frame.
            AssertSuccess(output_->CreateVideoFrame(
                1920, 1080, 1920 * 2,
                bmdFormat8BitYUV, bmdFrameFlagDefault, &frame_
            ));

            // Start getting callback from the output object.
            AssertSuccess(output_->SetScheduledFrameCompletionCallback(this));

            // Enable video output with 1080i59.94.
            AssertSuccess(output_->EnableVideoOutput(
                bmdModeHD1080i5994, bmdVideoOutputFlagDefault
            ));

            // Prerolling with blank frames.
            for (auto i = 0; i < Prerolling; i++) ScheduleFrame(frame_);

            // Start scheduled playback.
            AssertSuccess(output_->StartScheduledPlayback(0, 1, 1));
        }

        void StopSending()
        {
            // Stop the output stream.
            output_->StopScheduledPlayback(0, nullptr, 1);
            output_->SetScheduledFrameCompletionCallback(nullptr);
            output_->DisableVideoOutput();

            // Release the internal objects.
            frame_->Release();
            frame_ = nullptr;
            output_->Release();
            output_ = nullptr;
        }

        void UpdateFrame(void* data)
        {
            // Abandon the ownership of the last frame.
            frame_->Release();

            // Allocate a new frame.
            AssertSuccess(output_->CreateVideoFrame(
                1920, 1080, 1920 * 2,
                bmdFormat8BitYUV, bmdFrameFlagDefault, &frame_
            ));

            // Copy the frame data.
            void* pointer;
            AssertSuccess(frame_->GetBytes(&pointer));
            std::memcpy(pointer, data, 1920 * 2 * 1080);
        }

        // IUnknown implementation

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

        // IDeckLinkVideoOutputCallback implementation

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

            ScheduleFrame(frame_);

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped() override
        {
            return S_OK;
        }

    private:

        const BMDTimeScale TimeScale = 60000;
        const int Prerolling = 3;

        std::atomic<ULONG> refCount_;
        IDeckLinkOutput* output_;
        IDeckLinkMutableVideoFrame* frame_;
        uint64_t frameCount_;

        void ScheduleFrame(IDeckLinkVideoFrame* frame)
        {
            auto time = static_cast<BMDTimeValue>(TimeScale * frameCount_ * 2 / 59.94);
            auto duration = static_cast<BMDTimeValue>(TimeScale * 2 / 59.94);
            output_->ScheduleVideoFrame(frame, time, duration, TimeScale);
            frameCount_++;
        }
    };
}
