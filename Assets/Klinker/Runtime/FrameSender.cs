// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

using UnityEngine;
using UnityEngine.Rendering;
using System.Collections;
using System.Collections.Generic;

namespace Klinker
{
    // Frame sender class
    // Captures rendered frames from an attached camera and feeds them to a
    // DeckLink device.
    [AddComponentMenu("Klinker/Frame Sender")]
    [RequireComponent(typeof(Camera))]
    public sealed class FrameSender : MonoBehaviour
    {
        #region Editable attributes

        [SerializeField] int _deviceSelection = 0;
        [SerializeField] int _formatSelection = 0;
        [SerializeField, Range(1, 6)] int _queueLength = 3;
        [SerializeField] bool _lowLatencyMode = false;
        [SerializeField] bool _isMaster = false;

        #endregion

        #region Runtime properties

        static public FrameSender master { get; private set; }

        public long frameDuration { get {
            return _plugin?.FrameDuration ?? 0;
        } }

        public long timecodeFlicks { get {
            return _frameCount * _plugin.FrameDuration;
        } }

        public double timecodeSeconds { get {
            return (double)timecodeFlicks / Util.FlicksPerSecond;
        } }

        public int timecodeFrames { get {
            return (int)_frameCount;
        } }

        public bool isReferenceLocked { get {
            return _plugin?.IsReferenceLocked ?? false;
        } }

        #endregion

        #region Target camera accessors (for internal use)

        Camera _targetCamera;

        Camera TargetCamera { get {
            if (_targetCamera == null) _targetCamera = GetComponent<Camera>();
            return _targetCamera;
        } }

        RenderTextureFormat TargetFormat { get {
            return TargetCamera.allowHDR ?
                RenderTextureFormat.DefaultHDR : RenderTextureFormat.Default;
        } }

        int TargetAALevel { get {
            return TargetCamera.allowMSAA ? QualitySettings.antiAliasing : 1;
        } }

        #endregion

        #region Frame format conversion

        Material _subsampler;
        RenderTexture _oddField;

        RenderTexture EncodeFrame(RenderTexture source)
        {
            var dim = _plugin.FrameDimensions;

            // Progressive mode: Simply applies the chroma subsampler.
            if (_plugin.IsProgressive)
            {
                var frame = RenderTexture.GetTemporary(dim.x / 2, dim.y);
                Graphics.Blit(source, frame, _subsampler, 0);
                return frame;
            }

            // Interlace mode

            if (_oddField == null)
            {
                // Odd field: Make a copy of this frame to _oddField.
                _oddField = RenderTexture.GetTemporary(dim.x, dim.y);
                Graphics.Blit(source, _oddField);
                return null;
            }
            else
            {
                // Even field: Interlacing and chroma subsampling
                var frame = RenderTexture.GetTemporary(dim.x / 2, dim.y);
                _subsampler.SetTexture("_FieldTex", _oddField);
                Graphics.Blit(source, frame, _subsampler, 1);

                // Release the odd field.
                RenderTexture.ReleaseTemporary(_oddField);
                _oddField = null;

                return frame;
            }
        }

        #endregion

        #region Frame readback queue

        Queue<AsyncGPUReadbackRequest> _frameQueue = new Queue<AsyncGPUReadbackRequest>();

        void PushFrame(RenderTexture frame)
        {
            _frameQueue.Enqueue(AsyncGPUReadback.Request(frame));
        }

        void ProcessFrameQueue(bool sync)
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
                    // sync = off: Break if the request hasn't been completed.
                    if (!frame.done) break;
                }

                // Feed the frame data to the plugin.
                _plugin.FeedFrame(frame.GetData<byte>(), timecodeFlicks);
                _frameCount++;

                _frameQueue.Dequeue();
            }
        }

        #endregion

        #region Private members

        SenderPlugin _plugin;
        RenderTexture _targetRT;
        GameObject _blitter;
        long _frameCount;
        DropDetector _dropDetector;

        #endregion

        #region MonoBehaviour implementation

        IEnumerator Start()
        {
            // Internal objects initialization
            if (_isMaster)
                _plugin = SenderPlugin.
                    CreateManualSender(_deviceSelection, _formatSelection);
            else
                _plugin = SenderPlugin.
                    CreateAsyncSender(_deviceSelection, _formatSelection, _queueLength);

            _subsampler = new Material(Shader.Find("Hidden/Klinker/Subsampler"));
            _dropDetector = new DropDetector(gameObject.name);

            var dim = _plugin.FrameDimensions;

            // If the target camera doesn't have a target texture, create
            // a RT and set it as a target texture. Also create a blitter
            // object to keep frames presented on the screen.
            if (TargetCamera.targetTexture == null)
            {
                _targetRT = new RenderTexture(dim.x, dim.y, 24, TargetFormat);
                _targetRT.antiAliasing = TargetAALevel;
                TargetCamera.targetTexture = _targetRT;
                _blitter = Blitter.CreateInstance(TargetCamera);
            }
            else
            {
                // Make sure if the target texture has the correct dimensions.
                var rt = TargetCamera.targetTexture;
                if (rt.width != dim.x || rt.height != dim.y)
                    Debug.LogError(
                        "Target texture size mismatch. It should be " +
                        dim.x + " x " + dim.y
                    );
            }

            if (!_isMaster) yield break;

            // Master sender coroutine

            // Promote this instance to master.
            // Check the frame duration if there is another master.
            var duration = _plugin.FrameDuration;
            if (master != null && master.frameDuration != duration)
                Debug.LogError(
                    "Master frame rate mismatch. When using multiple master " +
                    "senders, they should have the exact same frame rate."
                );
            master = this;

            // Calculate the frame rate.
            // We prefer using ceiling values (i.e. 59.94 => 60)
            var fps = (int)((Util.FlicksPerSecond + duration - 1) / duration);
            if (!_plugin.IsProgressive) fps *= 2;

            // Set target frame rate (using captureFramerate) and stop v-sync.
            Time.captureFramerate = fps;
            QualitySettings.vSyncCount = 0;

            // Wait for sender completion every end-of-frame.
            var qlen = _queueLength;
            for (var eof = new WaitForEndOfFrame();;)
            {
                yield return eof;
                if (_frameCount < qlen) continue;
                _plugin.WaitCompletion(_frameCount - qlen);
            }
        }

        void OnDestroy()
        {
            if (_isMaster) master = null;

            Util.Destroy(_subsampler);

            if (_oddField != null) RenderTexture.ReleaseTemporary(_oddField);

            _plugin?.Dispose();

            if (_targetRT != null)
            {
                TargetCamera.targetTexture = null;
                Util.Destroy(_targetRT);
            }

            Util.Destroy(_blitter);

            // We don't have to care about the frame queue.
            // It will be automatically disposed.
        }

        void Update()
        {
            if (_plugin == null) return;

            var frame = EncodeFrame(TargetCamera.targetTexture);

            if (frame != null)
            {
                PushFrame(frame);
                RenderTexture.ReleaseTemporary(frame);
            }

            ProcessFrameQueue(_lowLatencyMode);

            _dropDetector.Update(_plugin.DropCount);
        }

        #endregion
    }
}
