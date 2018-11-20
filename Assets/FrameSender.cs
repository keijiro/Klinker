using Unity.Collections.LowLevel.Unsafe;
using UnityEngine;
using UnityEngine.Rendering;
using System;
using System.Collections.Generic;

namespace Klinker
{
    [RequireComponent(typeof(Camera))]
    public class FrameSender : MonoBehaviour
    {
        #region Private members

        IntPtr _plugin;

        Queue<AsyncGPUReadbackRequest> _frameQueue
            = new Queue<AsyncGPUReadbackRequest>();

        Material _encoderMaterial;

        public bool IsReferenceLocked { get {
            return _plugin != IntPtr.Zero &&
                PluginEntry.IsSenderReferenceLocked(_plugin) != 0;
        } }

        void Start()
        {
            _plugin = PluginEntry.CreateSender();
            _encoderMaterial = new Material(Shader.Find("Hidden/Klinker/Encoder"));
        }

        void OnDestroy()
        {
            PluginEntry.DestroySender(_plugin);
            Destroy(_encoderMaterial);
        }

        void Update()
        {
            while (_frameQueue.Count > 0)
            {
                var frame = _frameQueue.Peek();

                // Skip error frames.
                if (frame.hasError)
                {
                    Debug.LogWarning("GPU readback error was detected.");
                    _frameQueue.Dequeue();
                    continue;
                }

                // Break when found a frame that hasn't been read back yet.
                if (!frame.done) break;

                // Okay, we're going to send this frame.

                // Feed the frame data to the sender.
                unsafe {
                    PluginEntry.UpdateSenderFrame(
                        _plugin,
                        (IntPtr)frame.GetData<Byte>().GetUnsafeReadOnlyPtr()
                    );
                }

                // Done. Remove the frame from the queue.
                _frameQueue.Dequeue();
            }
        }

        void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            var tempRT = RenderTexture.GetTemporary(1920 / 2, 1080);
            Graphics.Blit(source, tempRT, _encoderMaterial, 0);
            _frameQueue.Enqueue(AsyncGPUReadback.Request(tempRT));
            Graphics.Blit(tempRT, destination);
            RenderTexture.ReleaseTemporary(tempRT);
        }

        #endregion
    }
}
