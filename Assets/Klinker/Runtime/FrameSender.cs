using UnityEngine;
using UnityEngine.Rendering;
using System.Collections;
using System.Collections.Generic;

namespace Klinker
{
    [AddComponentMenu("Klinker/Frame Sender")]
    [RequireComponent(typeof(Camera))]
    public class FrameSender : MonoBehaviour
    {
        #region Editable attributes

        [SerializeField] int _deviceSelection = 0;
        [SerializeField] int _formatSelection = 0;
        [SerializeField, Range(0, 10)] int _queueLength = 2;
        [SerializeField] bool _lowLatencyMode = false;
        [SerializeField] bool _isMaster = false;

        #endregion

        #region Public properties

        public bool isReferenceLocked { get {
            return _plugin?.IsReferenceLocked ?? false;
        } }

        #endregion

        #region Private members

        SenderPlugin _plugin;
        Queue<AsyncGPUReadbackRequest> _queue = new Queue<AsyncGPUReadbackRequest>();
        RenderTexture _fielding;
        Material _material;
        ulong _frameCount;

        void ProcessQueue(bool sync)
        {
            while (_queue.Count > 0)
            {
                var frame = _queue.Peek();

                // Skip error frames.
                if (frame.hasError)
                {
                    Debug.LogWarning("GPU readback error was detected.");
                    _queue.Dequeue();
                    continue;
                }

                if (sync)
                {
                    // sync = on: Wait for completion every time.
                    frame.WaitForCompletion();
                }
                else
                {
                    // sync = off: Break if the request hasn't been completed.
                    if (!frame.done) break;
                }

                // Feed the frame data to the plugin.
                _plugin.EnqueueFrame(frame.GetData<byte>());
                _frameCount++;

                _queue.Dequeue();
            }
        }

        #endregion

        #region MonoBehaviour implementation

        IEnumerator Start()
        {
            _plugin = new SenderPlugin(_deviceSelection, _formatSelection);
            _material = new Material(Shader.Find("Hidden/Klinker/Encoder"));

            if (_isMaster)
            {
                var fps = Mathf.CeilToInt(_plugin.FrameRate);
                if (_plugin.IsProgressive) fps *= 2;
                Time.captureFramerate = fps;

                var eof = new WaitForEndOfFrame();

                while (true)
                {
                    yield return eof;
                    if (_frameCount > 10)
                        _plugin.WaitCompletion(_frameCount - (ulong)_queueLength);
                }
            }
        }

        void OnDestroy()
        {
            _plugin.Dispose();
            Destroy(_material);
        }

        void Update()
        {
            // Normal mode: Process the frame queue without blocking.
            if (!_lowLatencyMode) ProcessQueue(false);
        }

        void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            var dimensions = _plugin.FrameDimensions;

            RenderTexture frame = null;

            if (_plugin.IsProgressive)
            {
                // Progressive mode: Request readback every frame.
                frame = RenderTexture.GetTemporary(dimensions.x / 2, dimensions.y);
                Graphics.Blit(source, frame, _material, 0);
            }
            else
            {
                // Interlace mode
                if (_fielding == null)
                {
                    // Odd frame: Make a copy of this frame with a fielding buffer.
                    _fielding = RenderTexture.GetTemporary(source.width, source.height);
                    Graphics.Blit(source, _fielding);
                }
                else
                {
                    // Even frame: Interlace and request readback.
                    frame = RenderTexture.GetTemporary(dimensions.x / 2, dimensions.y);
                    _material.SetTexture("_FieldTex", _fielding);
                    Graphics.Blit(source, frame, _material, 1);

                    // Release the fielding buffer.
                    RenderTexture.ReleaseTemporary(_fielding);
                    _fielding = null;
                }
            }

            if (frame != null)
            {
                // Async readback request
                _queue.Enqueue(AsyncGPUReadback.Request(frame));
                RenderTexture.ReleaseTemporary(frame);

                // Low-latency mode: Process the queued request immediately.
                if (_lowLatencyMode) ProcessQueue(true);
            }

            // Through blit
            Graphics.Blit(source, destination);
        }

        #endregion
    }
}
