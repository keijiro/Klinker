using UnityEngine;
using UnityEditor;
using System.Linq;

namespace Klinker
{
    [CustomEditor(typeof(FrameSender))]
    public sealed class FrameSenderEditor : Editor
    {
        SerializedProperty _deviceSelection;
        SerializedProperty _formatSelection;
        SerializedProperty _queueLength;
        SerializedProperty _lowLatencyMode;
        SerializedProperty _isMaster;

        GUIContent[] _deviceLabels;
        GUIContent[] _formatLabels;
        int[] _deviceValues;
        int[] _formatValues;

        readonly GUIContent _labelDevice = new GUIContent("Device");
        readonly GUIContent _labelFormat = new GUIContent("Format");

        void CacheFormats(int deviceIndex)
        {
            var formats = DeviceManager.GetOutputFormatNames(deviceIndex);
            _formatLabels = formats.Select((x) => new GUIContent(x)).ToArray();
            _formatValues = Enumerable.Range(0, formats.Length).ToArray();
        }

        public override bool RequiresConstantRepaint()
        {
            return Application.isPlaying;
        }

        void OnEnable()
        {
            _deviceSelection = serializedObject.FindProperty("_deviceSelection");
            _formatSelection = serializedObject.FindProperty("_formatSelection");
            _queueLength = serializedObject.FindProperty("_queueLength");
            _lowLatencyMode = serializedObject.FindProperty("_lowLatencyMode");
            _isMaster = serializedObject.FindProperty("_isMaster");

            // Scan all available devices.
            var devices = DeviceManager.GetDeviceNames();
            _deviceLabels = devices.Select((x) => new GUIContent(x)).ToArray();
            _deviceValues = Enumerable.Range(0, devices.Length).ToArray();

            CacheFormats(_deviceSelection.intValue);
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            if (_deviceLabels.Length > 0)
            {
                // Device selector
                EditorGUI.BeginChangeCheck();
                EditorGUILayout.IntPopup(_deviceSelection, _deviceLabels, _deviceValues, _labelDevice);
                if (EditorGUI.EndChangeCheck()) CacheFormats(_deviceSelection.intValue);

                // Format selector
                EditorGUILayout.IntPopup(_formatSelection, _formatLabels, _formatValues, _labelFormat);
            }
            else
            {
                EditorGUILayout.HelpBox("No capture device found.", MessageType.Info);
            }

            // Options
            EditorGUILayout.PropertyField(_queueLength);
            EditorGUILayout.PropertyField(_lowLatencyMode);
            EditorGUILayout.PropertyField(_isMaster);

            // Genlock status display
            var genlocked = ((FrameSender)target).isReferenceLocked;
            EditorGUILayout.LabelField("Reference Status", genlocked ? "Genlock Enabled" : "-");

            serializedObject.ApplyModifiedProperties();
        }
    }
}
