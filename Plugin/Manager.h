#pragma once

#include "Common.h"
#include "Receiver.h"

namespace klinker
{
    class Manager final
    {
    public:

        Manager()
        {
            // Retrieve the first DeckLink device via an iterator.
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

            // Frame receiver
            receiver_ = new Receiver(input_);
            AssertSuccess(input_->SetCallback(receiver_));
        }

        ~Manager()
        {
            input_->Release();
            receiver_->Release();
        }
        
        void Run()
        {
            // Enable the video input with a default video mode
            // (it will be changed by input mode detection).
            AssertSuccess(input_->EnableVideoInput(
                bmdModeHD720p60, bmdFormat10BitRGB,
                0
            ));

            // Start an input stream.
            AssertSuccess(input_->StartStreams());

            // Stop the input stream.
            AssertSuccess(input_->StopStreams());
            receiver_->StopReceiving();

            // Disable the video input/output.
            input_->DisableVideoInput();
        }
    private:

        IDeckLinkInput* input_;
        Receiver* receiver_;
    };
}