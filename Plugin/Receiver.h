#pragma once

#include "Common.h"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace klinker
{
    class Receiver final : public IDeckLinkInputCallback
    {
    public:

        //
        // Constructor/destructor
        //

        Receiver()
            : refCount_(1), id_(GenerateID()), frameWidth_(0), frameHeight_(0)
        {
            // Register itself to the id-instance map.
            GetInstanceMap()[id_] = this;
        }

        ~Receiver()
        {
            // Unregister itself from the id-instance map.
            GetInstanceMap().erase(id_);
        }

        //
        // Controller methods
        //

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
            // We assume no one uses NTSC 23.98 and expect that
            // it'll be changed in the initial frame. #bad_code
            AssertSuccess(input_->EnableVideoInput(
                bmdModeNTSC2398, bmdFormat8BitYUV,
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

        //
        // Accessor functions
        //

        unsigned int GetID() const
        {
            return id_;
        }

        int GetFrameWidth() const
        {
            return frameWidth_;
        }

        int GetFrameHeight() const
        {
            return frameHeight_;
        }

        //
        // Buffer operations
        //

        uint8_t* LockBuffer()
        {
            frameLock_.lock();
            return frameBuffer_.data();
        }

        void UnlockBuffer()
        {
            frameLock_.unlock();
        }

        //
        // Static functions
        //

        static Receiver* GetInstanceFromID(unsigned int id)
        {
            auto map = GetInstanceMap();
            auto itr = map.find(id);
            return itr != map.end() ? itr->second : nullptr;
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
            // Determine the frame dimensions.
            frameWidth_ = newDisplayMode->GetWidth();
            frameHeight_ = newDisplayMode->GetHeight();

            // Change the video input format as notified.
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
                std::lock_guard<std::mutex> lock(frameLock_);

                // Resize the buffer to store the arrived frame.
                auto size = videoFrame->GetRowBytes() * frameHeight_;
                frameBuffer_.resize(size);

                // Copy the frame data. #there_might_be_a_better_way
                void* source;
                AssertSuccess(videoFrame->GetBytes(&source));
                std::memcpy(frameBuffer_.data(), source, size);
            }
            return S_OK;
        }

    private:

        // Internal state
        std::atomic<ULONG> refCount_;
        unsigned int id_;
        IDeckLinkInput* input_;

        // Frame data
        std::vector<uint8_t> frameBuffer_;
        int frameWidth_, frameHeight_;
        std::mutex frameLock_;

        //
        // Utility functions
        //

        static BMDPixelFormat FormatFlagsToPixelFormat(BMDDetectedVideoInputFormatFlags flags)
        {
            return flags == bmdDetectedVideoInputRGB444 ? bmdFormat8BitARGB : bmdFormat8BitYUV;
        }

        //
        // ID-instance mapping
        //

        static unsigned int GenerateID()
        {
            static unsigned int counter;
            return counter++;
        }

        static std::unordered_map<unsigned int, Receiver*>& GetInstanceMap()
        {
            static std::unordered_map<unsigned int, Receiver*> map;
            return map;
        }
    };
}
