#pragma once

#include "Common.h"
#include <atomic>
#include <mutex>
#include <tuple>
#include <vector>

namespace klinker
{
    class Receiver final : public IDeckLinkInputCallback
    {
    public:

        // Constructor/destructor

        Receiver()
            : refCount_(1), frameSize_({0, 0})
        {
        }

        // Controller methods

        void StartReceiving()
        {
            // Retrieve the first DeckLink device using an iterator.
            IDeckLinkIterator* iterator;
            AssertSuccess(CoCreateInstance(
                CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL,
                IID_IDeckLinkIterator, reinterpret_cast<void**>(&iterator)
            ));

            IDeckLink* device;
            AssertSuccess(iterator->Next(&device));

            // Retrieve an input interface from the device.
            AssertSuccess(device->QueryInterface(
                IID_IDeckLinkInput, reinterpret_cast<void**>(&input_)
            ));

            // These references are no longer needed.
            iterator->Release();
            device->Release();

            // Register itself as a callback.
            AssertSuccess(input_->SetCallback(this));

            // Enable the video input with a default mode.
            // We assume no one uses PAL and expect that
            // it'll be changed in the initial frame. #bad_code
            AssertSuccess(input_->EnableVideoInput(
                bmdModePAL, bmdFormat8BitYUV,
                bmdVideoInputEnableFormatDetection
            ));

            // Start an input stream.
            AssertSuccess(input_->StartStreams());
        }

        void StopReceiving()
        {
            // Stop, disable and release the input stream.
            input_->StopStreams();
            input_->DisableVideoInput();
            input_->SetCallback(nullptr);
            input_->Release();
        }

        // Accessor functions

        auto GetFrameDimensions() const
        {
            return frameSize_;
        }

        uint8_t* LockData()
        {
            frameLock_.lock();
            return frameData_.data();
        }

        void UnlockData()
        {
            frameLock_.unlock();
        }

        // IUnknown implementation

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

        // IDeckLinkInputCallback implementation

        HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
            BMDVideoInputFormatChangedEvents events,
            IDeckLinkDisplayMode* mode,
            BMDDetectedVideoInputFormatFlags flags
        ) override
        {
            // Determine the frame dimensions.
            frameSize_ = { mode->GetWidth(), mode->GetHeight() };

            {
                std::lock_guard<std::mutex> lock(frameLock_);
                frameData_.resize(mode->GetWidth() * 2 * mode->GetHeight());
            }

            // Change the video input format as notified.
            input_->PauseStreams();
            input_->EnableVideoInput(
                mode->GetDisplayMode(),
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
            if (videoFrame != nullptr)
            {
                std::lock_guard<std::mutex> lock(frameLock_);

                // Resize the buffer to store the arrived frame.
                auto size = videoFrame->GetRowBytes() * videoFrame->GetHeight();
                frameData_.resize(size);

                // Copy the frame data. #there_might_be_a_better_way
                void* source;
                AssertSuccess(videoFrame->GetBytes(&source));
                std::memcpy(frameData_.data(), source, size);
            }
            return S_OK;
        }

    private:

        std::atomic<ULONG> refCount_;
        IDeckLinkInput* input_;
        std::vector<uint8_t> frameData_;
        std::tuple<int, int> frameSize_;
        std::mutex frameLock_;
    };
}
