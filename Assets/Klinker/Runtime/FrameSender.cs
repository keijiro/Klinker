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
        #region Editable attributes

        [SerializeField] int _deviceSelection = 0;
        [SerializeField] int _formatSelection = 0;
        [SerializeField, Range(0, 10)] int _queueLength = 2;
        [SerializeField] bool _lowLatencyMode = false;

        #endregion

        #region Public properties

        public bool IsReferenceLocked { get {
            return _plugin != IntPtr.Zero && PluginEntry.IsSenderReferenceLocked(_plugin) != 0;
        } }

        #endregion

        #region Private members

        IntPtr _plugin;
        Queue<AsyncGPUReadbackRequest> _frameQueue = new Queue<AsyncGPUReadbackRequest>();
        Material _encoderMaterial;

        void ProcessQueue(bool sync)
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

                if (sync)
                {
                    // sync = on: Wait for completion every time.
                    frame.WaitForCompletion();
                }
                else
                {
                    // sync = off: Break if a request hasn't been completed.
                    if (!frame.done) break;
                }

                // Feed the frame data to the plugin.
                unsafe {
                    var pointer = frame.GetData<Byte>().GetUnsafeReadOnlyPtr();
                    PluginEntry.UpdateSenderFrame(_plugin, (IntPtr)pointer);
                }

                _frameQueue.Dequeue();
            }
        }

        #endregion

        #region MonoBehaviour implementation

        void Start()
        {
            _plugin = PluginEntry.CreateSender(_deviceSelection, _formatSelection, _queueLength);
            _encoderMaterial = new Material(Shader.Find("Hidden/Klinker/Encoder"));
        }

        void OnDestroy()
        {
            PluginEntry.DestroySender(_plugin);
            Destroy(_encoderMaterial);
        }

        void Update()
        {
            // Normal mode: Process the frame queue without blocking.
            if (!_lowLatencyMode) ProcessQueue(false);
        }

        void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            // Blit to a temporary RT and request a readback on it.
            var tempRT = RenderTexture.GetTemporary(
                PluginEntry.GetSenderFrameWidth(_plugin) / 2,
                PluginEntry.GetSenderFrameHeight(_plugin)
            );
            Graphics.Blit(source, tempRT, _encoderMaterial, 0);
            _frameQueue.Enqueue(AsyncGPUReadback.Request(tempRT));
            RenderTexture.ReleaseTemporary(tempRT);

            // Low-latency mode: Process the queued request immediately.
            if (_lowLatencyMode) ProcessQueue(true);

            // Through blit
            Graphics.Blit(source, destination);
        }

        #endregion
    }
}
