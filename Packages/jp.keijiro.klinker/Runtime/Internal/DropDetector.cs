// Klinker - Blackmagic DeckLink plugin for Unity
// https://github.com/keijiro/Klinker

using UnityEngine;

namespace Klinker
{
    // Frame drop detector class
    // Detects frame drops and show warning messages.
    class DropDetector
    {
        string _name;
        int _lastCount;
        float _suppress;

        public DropDetector(string name)
        {
            _name = name;
        }

        public void Update(int count)
        {
            if (count > _lastCount)
            {
                Warn();
                _lastCount = count;
            }
            else
            {
                _suppress = Mathf.Max(0, _suppress - Time.deltaTime);
            }
        }

        public void Warn()
        {
            if (Time.time < 2) return;

            if (_suppress == 0)
            {
                Debug.LogWarning(
                    "Frame drop was detected in [" + _name + "]. To prevent " +
                    "drops, it's recommended to increase the queue length, " +
                    "reduce the resolution or the frame rate."
                );
            }

            _suppress = 1.0f;
        }
    }
}
