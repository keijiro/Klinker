using UnityEngine;
using UnityEditor;

namespace Klinker
{
    [CanEditMultipleObjects]
    [CustomEditor(typeof(FrameSender))]
    public sealed class FrameSenderEditor : Editor
    {
        SerializedProperty _bufferLength;

        public override bool RequiresConstantRepaint()
        {
            return Application.isPlaying;
        }

        void OnEnable()
        {
            _bufferLength = serializedObject.FindProperty("_bufferLength");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

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
