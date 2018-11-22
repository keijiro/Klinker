using UnityEngine;
using UnityEditor;
using System.Linq;

namespace Klinker
{
    [CustomEditor(typeof(FrameReceiver))]
    public sealed class FrameReceiverEditor : Editor
    {
        SerializedProperty _deviceSelection;

        GUIContent[] _deviceLabels;
        int[] _deviceOptions;

        readonly GUIContent _labelDevice = new GUIContent("Device");

        public override bool RequiresConstantRepaint()
        {
            return Application.isPlaying;
        }

        void OnEnable()
        {
            _deviceSelection = serializedObject.FindProperty("_deviceSelection");

            var devices = DeviceManager.GetDeviceNames();
            _deviceLabels = devices.Select((x) => new GUIContent(x)).ToArray();
            _deviceOptions = Enumerable.Range(0, devices.Length).ToArray();
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.IntPopup
                (_deviceSelection, _deviceLabels, _deviceOptions, _labelDevice);

            var receiver = (FrameReceiver)target;
            EditorGUILayout.LabelField("Format", receiver.FormatName);

            serializedObject.ApplyModifiedProperties();
        }
    }
}
