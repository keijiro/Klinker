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
            : refCount_(1),
              input_(nullptr), displayMode_(nullptr),
              frameSize_({0, 0})
        {
        }

        ~Receiver()
        {
            // Internal objects should have been released.
            assert(input_ == nullptr);
            assert(displayMode_ == nullptr);
        }

        #pragma endregion

        #pragma region Accessor methods

        auto GetFrameDimensions() const
        {
            return frameSize_;
        }

        std::size_t CalculateFrameDataSize() const
        {
            return (std::size_t)2 * std::get<0>(frameSize_) * std::get<1>(frameSize_);
        }

        const uint8_t* LockFrameData()
        {
            frameMutex_.lock();
            return frameData_.data();
        }

        void UnlockFrameData()
        {
            frameMutex_.unlock();
        }

        BSTR RetrieveFormatName() const
        {
            assert(displayMode_ != nullptr);
            std::lock_guard<std::mutex> lock(frameMutex_);
            BSTR name;
            AssertSuccess(displayMode_->GetName(&name));
            return name;
        }

        #pragma endregion

        #pragma region Public methods

        void StartReceiving(int deviceIndex, int formatIndex)
        {
            assert(input_ == nullptr);
            assert(displayMode_ == nullptr);

            // Initialize a video input.
            InitializeInput(deviceIndex, formatIndex);

            // Allocate an initial frame memory.
            frameSize_ = { displayMode_->GetWidth(), displayMode_->GetHeight() };
            frameData_.resize(CalculateFrameDataSize());

            // Start getting callback from the video input.
            AssertSuccess(input_->SetCallback(this));

            // Enable the video input with format detection.
            AssertSuccess(input_->EnableVideoInput(
                displayMode_->GetDisplayMode(),
                bmdFormat8BitYUV,
                bmdVideoInputEnableFormatDetection
            ));

            // Start an input stream.
            AssertSuccess(input_->StartStreams());
        }

        void StopReceiving()
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
                std::lock_guard<std::mutex> lock(frameMutex_);

                // Change the display mode.
                displayMode_->Release();
                displayMode_ = mode;
                mode->AddRef();

                // Resize the frame memory.
                frameSize_ = { displayMode_->GetWidth(), displayMode_->GetHeight() };
                frameData_.resize(CalculateFrameDataSize());
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
            if (videoFrame != nullptr)
            {
                std::lock_guard<std::mutex> lock(frameMutex_);

                // Calculate the size of the given video frame.
                auto size = videoFrame->GetRowBytes() * videoFrame->GetHeight();

                // Do nothing if the size mismatches.
                if (size != CalculateFrameDataSize()) return S_OK;

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
        IDeckLinkDisplayMode* displayMode_;

        std::vector<uint8_t> frameData_;
        std::tuple<int, int> frameSize_;
        mutable std::mutex frameMutex_;

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

            // Cleaning up
            dmIterator->Release();
        }

        #pragma endregion
    };
}
