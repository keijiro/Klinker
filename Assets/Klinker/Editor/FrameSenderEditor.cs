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
        SerializedProperty _bufferLength;

        GUIContent[] _deviceLabels;
        int[] _deviceOptions;

        GUIContent[] _formatLabels;
        int[] _formatOptions;

        readonly GUIContent _labelDevice = new GUIContent("Device");
        readonly GUIContent _labelFormat = new GUIContent("Format");

        void CacheFormats(int deviceIndex)
        {
            var formats = DeviceManager.GetOutputFormatNames(deviceIndex);
            _formatLabels = formats.Select((x) => new GUIContent(x)).ToArray();
            _formatOptions = Enumerable.Range(0, formats.Length).ToArray();
        }

        public override bool RequiresConstantRepaint()
        {
            return Application.isPlaying;
        }

        void OnEnable()
        {
            _deviceSelection = serializedObject.FindProperty("_deviceSelection");
            _formatSelection = serializedObject.FindProperty("_formatSelection");
            _bufferLength = serializedObject.FindProperty("_bufferLength");

            var devices = DeviceManager.GetDeviceNames();
            _deviceLabels = devices.Select((x) => new GUIContent(x)).ToArray();
            _deviceOptions = Enumerable.Range(0, devices.Length).ToArray();

            CacheFormats(_deviceSelection.intValue);
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUI.BeginChangeCheck();

            EditorGUILayout.IntPopup
                (_deviceSelection, _deviceLabels, _deviceOptions, _labelDevice);

            if (EditorGUI.EndChangeCheck())
                CacheFormats(_deviceSelection.intValue);

            EditorGUILayout.IntPopup
                (_formatSelection, _formatLabels, _formatOptions, _labelFormat);

            EditorGUILayout.PropertyField(_bufferLength);

            if (targets.Length == 1)
            {
                var sender = (FrameSender)target;

                EditorGUILayout.LabelField(
                    "Reference Status", 
                    sender.IsReferenceLocked ? "Genlock Enabled" : "-"
                );
            }

            serializedObject.ApplyModifiedProperties();
        }
    }
}
