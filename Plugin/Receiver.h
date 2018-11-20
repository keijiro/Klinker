#pragma once

#include "Common.h"
#include <atomic>
#include <mutex>
#include <tuple>
#include <vector>

namespace klinker
{
    // Frame receiver class
    // It also privately implements a video input callback interface.
    class Receiver final : private IDeckLinkInputCallback
    {
    public:

        #pragma region Constructor/destructor

        Receiver()
            : refCount_(1), input_(nullptr), frameSize_({0, 0})
        {
        }

        ~Receiver()
        {
            // Internal objects should have been released.
            assert(input_ == nullptr);
        }

        #pragma endregion

        #pragma region Controller methods

        void StartReceiving()
        {
            assert(input_ == nullptr);

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

            // Register itself as a video input callback object.
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
            assert(input_ != nullptr);

            // Stop, disable and release the input stream.
            input_->StopStreams();
            input_->DisableVideoInput();
            input_->SetCallback(nullptr);
            input_->Release();
            input_ = nullptr;
        }

        #pragma endregion

        #pragma region Accessor functions

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
            // Determine the frame dimensions.
            auto w = mode->GetWidth(), h = mode->GetHeight();
            frameSize_ = { w, h };

            // Expand/shrink the frame memory.
            frameLock_.lock();
            frameData_.resize((size_t)2 * w * h);
            frameLock_.unlock();

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

                // Frame data size in bytes
                auto size = videoFrame->GetRowBytes() * videoFrame->GetHeight();

                // Do nothing if the size mismatches.
                if (size != frameData_.size()) return S_OK;

                // Simply memcpy the content of the frame.
                void* source;
                AssertSuccess(videoFrame->GetBytes(&source));
                std::memcpy(frameData_.data(), source, size);
            }
            return S_OK;
        }

        #pragma endregion

    private:

        #pragma region Private members

        std::atomic<ULONG> refCount_;
        IDeckLinkInput* input_;
        std::tuple<int, int> frameSize_;
        std::vector<uint8_t> frameData_;
        std::mutex frameLock_;

        #pragma endregion
    };
}
