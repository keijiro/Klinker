#include "Common.h"
#include "Receiver.h"
#include "Sender.h"
#include "ObjectIDMap.h"
#include "Unity/IUnityRenderingExtensions.h"

namespace klinker
{
    ObjectIDMap<Receiver> receiverMap_;

    // Callback for texture update events
    void TextureUpdateCallback(int eventID, void* data)
    {
        auto event = static_cast<UnityRenderingExtEventType>(eventID);
        if (event == kUnityRenderingExtEventUpdateTextureBeginV2)
        {
            // UpdateTextureBegin
            auto params = reinterpret_cast<UnityRenderingExtTextureUpdateParamsV2*>(data);
            auto receiver = receiverMap_[params->userData];
            params->texData = receiver->LockData();
        }
        else if (event == kUnityRenderingExtEventUpdateTextureEndV2)
        {
            // UpdateTextureEnd
            auto params = reinterpret_cast<UnityRenderingExtTextureUpdateParamsV2*>(data);
            auto receiver = receiverMap_[params->userData];
            receiver->UnlockData();
        }
    }
}

extern "C" void UNITY_INTERFACE_EXPORT * CreateReceiver()
{
    auto receiver = new klinker::Receiver();
    klinker::receiverMap_.Add(receiver);
    receiver->StartReceiving();
    return receiver;
}

extern "C" void UNITY_INTERFACE_EXPORT DestroyReceiver(void* receiverPointer)
{
    auto receiver = reinterpret_cast<klinker::Receiver*>(receiverPointer);
    klinker::receiverMap_.Remove(receiver);
    receiver->StopReceiving();
    receiver->Release();
}

extern "C" unsigned int UNITY_INTERFACE_EXPORT GetReceiverID(void* receiver)
{
    return klinker::receiverMap_.GetID(reinterpret_cast<klinker::Receiver*>(receiver));
}

extern "C" int UNITY_INTERFACE_EXPORT GetReceiverFrameWidth(void* receiver)
{
    return std::get<0>(reinterpret_cast<klinker::Receiver*>(receiver)->GetFrameDimensions());
}

extern "C" int UNITY_INTERFACE_EXPORT GetReceiverFrameHeight(void* receiver)
{
    return std::get<1>(reinterpret_cast<klinker::Receiver*>(receiver)->GetFrameDimensions());
}

extern "C" void UNITY_INTERFACE_EXPORT * CreateSender()
{
    auto sender = new klinker::Sender();
    sender->StartSending();
    return sender;
}

extern "C" void UNITY_INTERFACE_EXPORT DestroySender(void* senderPointer)
{
    auto sender = reinterpret_cast<klinker::Sender*>(senderPointer);
    sender->StopSending();
    sender->Release();
}

extern "C" int UNITY_INTERFACE_EXPORT IsSenderReferenceLocked(void* senderPointer)
{
    auto sender = reinterpret_cast<klinker::Sender*>(senderPointer);
    return sender->IsReferenceLocked() ? 1 : 0;
}

extern "C" void UNITY_INTERFACE_EXPORT UpdateSenderFrame(void* senderPointer, void* data)
{
    auto sender = reinterpret_cast<klinker::Sender*>(senderPointer);
    sender->UpdateFrame(data);
}

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT GetTextureUpdateCallback()
{
    return klinker::TextureUpdateCallback;
}
