#include "Enumerator.h"
#include "ObjectIDMap.h"
#include "Receiver.h"
#include "Sender.h"
#include "Unity/IUnityRenderingExtensions.h"

#pragma region Local functions

namespace
{
    // ID-receiver map
    klinker::ObjectIDMap<klinker::Receiver> receiverMap_;

    // Callback for texture update events
    void TextureUpdateCallback(int eventID, void* data)
    {
        auto event = static_cast<UnityRenderingExtEventType>(eventID);
        if (event == kUnityRenderingExtEventUpdateTextureBeginV2)
        {
            // UpdateTextureBegin
            auto params = reinterpret_cast<UnityRenderingExtTextureUpdateParamsV2*>(data);
            auto receiver = receiverMap_[params->userData];
            if (receiver == nullptr) return;

            // Check if the size of the data matches.
            auto dataSize = params->width * params->height * params->bpp;
            if (receiver->CalculateFrameDataSize() != dataSize) return;

            // Lock the frame data for the update.
            params->texData = const_cast<uint8_t*>(receiver->LockOldestFrameData());
        }
        else if (event == kUnityRenderingExtEventUpdateTextureEndV2)
        {
            // UpdateTextureEnd
            auto params = reinterpret_cast<UnityRenderingExtTextureUpdateParamsV2*>(data);
            auto receiver = receiverMap_[params->userData];
            if (receiver == nullptr) return;

            // Unlock the frame data if it seems to be locked.
            if (params->texData != nullptr) receiver->UnlockOldestFrameData();
        }
    }
}

#pragma endregion

#pragma region Plugin common functions

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT GetTextureUpdateCallback()
{
    return TextureUpdateCallback;
}

#pragma endregion

#pragma region Enumeration plugin functions

namespace { klinker::Enumerator enumerator_; }

extern "C" int UNITY_INTERFACE_EXPORT RetrieveDeviceNames(void* names[], int maxCount)
{
    enumerator_.ScanDeviceNames();
    return enumerator_.CopyStringPointers(names, maxCount);
}

extern "C" int UNITY_INTERFACE_EXPORT RetrieveOutputFormatNames(int deviceIndex, void* names[], int maxCount)
{
    enumerator_.ScanOutputFormatNames(deviceIndex);
    return enumerator_.CopyStringPointers(names, maxCount);
}

#pragma endregion

#pragma region Receiver plugin functions

extern "C" void UNITY_INTERFACE_EXPORT * CreateReceiver(int device, int format)
{
    auto instance = new klinker::Receiver();
    receiverMap_.Add(instance);
    instance->Start(device, format);
    return instance;
}

extern "C" void UNITY_INTERFACE_EXPORT DestroyReceiver(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return;
    receiverMap_.Remove(instance);
    instance->Stop();
    instance->Release();
}

extern "C" unsigned int UNITY_INTERFACE_EXPORT GetReceiverID(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return 0;
    return receiverMap_.GetID(instance);
}

extern "C" int UNITY_INTERFACE_EXPORT GetReceiverFrameWidth(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return 0;
    return std::get<0>(instance->GetFrameDimensions());
}

extern "C" int UNITY_INTERFACE_EXPORT GetReceiverFrameHeight(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return 0;
    return std::get<1>(instance->GetFrameDimensions());
}

extern "C" std::int64_t UNITY_INTERFACE_EXPORT GetReceiverFrameDuration(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return 0;
    return instance->GetFrameDuration();
}

extern "C" int UNITY_INTERFACE_EXPORT IsReceiverProgressive(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return 0;
    return instance->IsProgressive() ? 1 : 0;
}

extern "C" void UNITY_INTERFACE_EXPORT * GetReceiverFormatName(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return nullptr;
    static BSTR name = nullptr;
    if (name != nullptr) SysFreeString(name);
    name = instance->RetrieveFormatName();
    return name;
}

extern "C" int UNITY_INTERFACE_EXPORT CountReceiverQueuedFrames(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return 0;
    return static_cast<int>(instance->CountQueuedFrames());
}

extern "C" void UNITY_INTERFACE_EXPORT DequeueReceiverFrame(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return;
    instance->DequeueFrame();
}

extern "C" unsigned int UNITY_INTERFACE_EXPORT GetReceiverTimecode(void* receiver)
{
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    if (instance == nullptr) return 0xffffffffL;
    return instance->GetOldestTimecode();
}

extern "C" int UNITY_INTERFACE_EXPORT CountDroppedReceiverFrames(void* receiver)
{
    if (receiver == nullptr) return 0;
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    return instance->CountDroppedFrames();
}

extern "C" const void UNITY_INTERFACE_EXPORT * GetReceiverError(void* receiver)
{
    if (receiver == nullptr) return nullptr;
    auto instance = reinterpret_cast<klinker::Receiver*>(receiver);
    const auto& error = instance->GetErrorString();
    return error.empty() ? nullptr : error.c_str();
}

#pragma endregion

#pragma region Sender plugin functions

extern "C" void UNITY_INTERFACE_EXPORT * CreateAsyncSender(int device, int format, int preroll)
{
    auto instance = new klinker::Sender();
    instance->StartAsyncMode(device, format, preroll);
    return instance;
}

extern "C" void UNITY_INTERFACE_EXPORT * CreateManualSender(int device, int format)
{
    auto instance = new klinker::Sender();
    instance->StartManualMode(device, format);
    return instance;
}

extern "C" void UNITY_INTERFACE_EXPORT DestroySender(void* sender)
{
    if (sender == nullptr) return;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    instance->Stop();
    instance->Release();
}

extern "C" int UNITY_INTERFACE_EXPORT GetSenderFrameWidth(void* sender)
{
    if (sender == nullptr) return 0;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    return std::get<0>(instance->GetFrameDimensions());
}

extern "C" int UNITY_INTERFACE_EXPORT GetSenderFrameHeight(void* sender)
{
    if (sender == nullptr) return 0;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    return std::get<1>(instance->GetFrameDimensions());
}

extern "C" std::int64_t UNITY_INTERFACE_EXPORT GetSenderFrameDuration(void* sender)
{
    if (sender == nullptr) return 0;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    return instance->GetFrameDuration();
}

extern "C" int UNITY_INTERFACE_EXPORT IsSenderProgressive(void* sender)
{
    if (sender == nullptr) return 0;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    return instance->IsProgressive() ? 1 : 0;
}

extern "C" int UNITY_INTERFACE_EXPORT IsSenderReferenceLocked(void* sender)
{
    if (sender == nullptr) return 0;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    return instance->IsReferenceLocked() ? 1 : 0;
}

extern "C" void UNITY_INTERFACE_EXPORT FeedFrameToSender(void* sender, void* frameData, unsigned int timecode)
{
    if (sender == nullptr) return;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    instance->FeedFrame(frameData, timecode);
}

extern "C" void UNITY_INTERFACE_EXPORT WaitSenderCompletion(void* sender, std::int64_t frameNumber)
{
    if (sender == nullptr) return;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    instance->WaitFrameCompletion(frameNumber);
}

extern "C" const int UNITY_INTERFACE_EXPORT CountDroppedSenderFrames(void* sender)
{
    if (sender == nullptr) return 0;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    return instance->CountDroppedFrames();
}

extern "C" const void UNITY_INTERFACE_EXPORT * GetSenderError(void* sender)
{
    if (sender == nullptr) return nullptr;
    auto instance = reinterpret_cast<klinker::Sender*>(sender);
    const auto& error = instance->GetErrorString();
    return error.empty() ? nullptr : error.c_str();
}

#pragma endregion
