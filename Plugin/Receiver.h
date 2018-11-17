#pragma once

#include "Common.h"
#include "MemoryBackedFrame.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace klinker
{
    //
    // Frame receiver class with frame input queue
    //
    class Receiver final : public IDeckLinkInputCallback
    {
    public:

        Receiver(IDeckLinkInput* input)
            : refCount_(1), input_(input), disabled_(false)
        {
            AssertSuccess(CoCreateInstance(
                CLSID_CDeckLinkVideoConversion, nullptr, CLSCTX_ALL,
                IID_IDeckLinkVideoConversion, reinterpret_cast<void**>(&converter_)
            ));
        }

        ~Receiver()
        {
            while (!frameQueue_.empty())
            {
                frameQueue_.front()->Release();
                frameQueue_.pop();
            }
            converter_->Release();
        }

        void StopReceiving()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            disabled_ = true;
            semaphore_.notify_all(); // Resume paused threads.
        }

        MemoryBackedFrame* PopFrameSync()
        {
            if (disabled_) return nullptr;

            // Immediately return the oldest entry if available.
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!frameQueue_.empty())
                {
                    auto frame = frameQueue_.front();
                    frameQueue_.pop();
                    return frame;
                }
            }

            if (disabled_) return nullptr;

            // Wait for a new frame arriving.
            std::unique_lock<std::mutex> lock(mutex_);
            semaphore_.wait(lock);

            if (disabled_) return nullptr;

            // Return the oldest entry.
            auto frame = frameQueue_.front();
            frameQueue_.pop();
            return frame;
        }

        //
        // IUnknown implementation
        //

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
            if (val == 0) delete this;
            return val;
        }

        //
        // IDeckLinkInputCallback implementation
        //

        HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
            BMDVideoInputFormatChangedEvents notificationEvents,
            IDeckLinkDisplayMode* newDisplayMode,
            BMDDetectedVideoInputFormatFlags detectedSignalFlags
        ) override
        {
            input_->PauseStreams();
            input_->EnableVideoInput(
                newDisplayMode->GetDisplayMode(),
                FormatFlagsToPixelFormat(detectedSignalFlags),
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
            if (videoFrame != nullptr)
            {
                // Push the arrived frame to the frame queue.
                auto frame = new MemoryBackedFrame(videoFrame, converter_);
                std::lock_guard<std::mutex> lock(mutex_);
                frameQueue_.push(frame);
                semaphore_.notify_all();
            }
            return S_OK;
        }

    private:

        std::atomic<ULONG> refCount_;

        IDeckLinkInput* input_;
        IDeckLinkVideoConversion* converter_;

        std::queue<MemoryBackedFrame*> frameQueue_;
        std::mutex mutex_;
        std::condition_variable semaphore_;
        bool disabled_;

        static BMDPixelFormat FormatFlagsToPixelFormat(BMDDetectedVideoInputFormatFlags flags)
        {
            return flags == bmdDetectedVideoInputRGB444 ? bmdFormat10BitRGB : bmdFormat10BitYUV;
        }
    };
}
