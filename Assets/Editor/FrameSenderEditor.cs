using UnityEngine;
using UnityEditor;

namespace Klinker
{
    [CanEditMultipleObjects]
    [CustomEditor(typeof(FrameSender))]
    public sealed class FrameSenderEditor : Editor
    {
        public override bool RequiresConstantRepaint()
        {
            return Application.isPlaying;
        }

        public override void OnInspectorGUI()
        {
            if (targets.Length == 1)
            {
                var sender = (FrameSender)target;

                EditorGUILayout.LabelField(
                    "Genlock Status", 
                    sender.IsReferenceLocked ? "Locked" : "-"
                );
            }
        }
    }
}
