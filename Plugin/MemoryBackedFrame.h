#pragma once

#include "Common.h"
#include <atomic>
#include <vector>

namespace klinker
{
    //
    // Custom frame class with ARGB 32-bit color format
    //
    class MemoryBackedFrame final : public IDeckLinkVideoFrame
    {
    public:

        MemoryBackedFrame(
            IDeckLinkVideoInputFrame* input,
            IDeckLinkVideoConversion* converter
        )
            : refCount_(1)
        {
            width_ = input->GetWidth();
            height_ = input->GetHeight();
            memory_.resize(width_ * height_);
            AssertSuccess(converter->ConvertFrame(input, this));
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

            if (iid == IID_IDeckLinkVideoFrame)
            {
                *ppv = (IDeckLinkVideoFrame*)this;
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
        // IDeckLinkVideoFrame implementation
        //

        long STDMETHODCALLTYPE GetWidth() override
        {
            return width_;
        }

        long STDMETHODCALLTYPE GetHeight() override
        {
            return height_;
        }

        long STDMETHODCALLTYPE GetRowBytes() override
        {
            return width_ * sizeof(uint32_t);
        }

        BMDPixelFormat STDMETHODCALLTYPE GetPixelFormat() override
        {
            return bmdFormat8BitARGB;
        }

        BMDFrameFlags STDMETHODCALLTYPE GetFlags() override
        {
            return bmdFrameFlagDefault;
        }

        HRESULT STDMETHODCALLTYPE GetBytes(void** buffer)
        {
            *buffer = memory_.data();
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetTimecode(BMDTimecodeFormat format, IDeckLinkTimecode** timecode) override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetAncillaryData(IDeckLinkVideoFrameAncillary** ancillary) override
        {
            return E_NOTIMPL;
        }

    private:

        std::atomic<ULONG> refCount_;
        std::vector<uint32_t> memory_;
        long width_;
        long height_;
    };
}
