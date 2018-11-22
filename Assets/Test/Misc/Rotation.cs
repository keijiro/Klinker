using UnityEngine;

namespace Klinker.Test
{
    public class Rotation : MonoBehaviour
    {
        [SerializeField] Vector3 _velocity = new Vector3(0, 90, 0);

        void Update()
        {
            transform.localRotation =
                Quaternion.Euler(_velocity * Time.deltaTime) *
                transform.localRotation;
        }
    }
}
