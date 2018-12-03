using System.Collections;
using UnityEngine;

public class Flasher : MonoBehaviour
{
    [SerializeField] Color _color1 = Color.black;
    [SerializeField] Color _color2 = Color.white;
    [SerializeField, Range(1, 10)] int _interval = 1;

    IEnumerator Start()
    {
        var cam = GetComponent<Camera>();

        while (true)
        {
            cam.backgroundColor = _color1;
            for (var i = 0; i < _interval; i++) yield return null;

            cam.backgroundColor = _color2;
            for (var i = 0; i < _interval; i++) yield return null;
        }
    }
}
