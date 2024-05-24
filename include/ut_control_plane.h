#include <ut_kvp.h>

#define UT_CONTROL_PLANE_MAX_KEY_SIZE (64)
#define UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES (32)

/**! Handle to a control plane instance. */
typedef void ut_controlPlane_instance_t;

/* Callback function that will be triggered */
typedef void (*ut_control_callback_t)( char *key, ut_kvp_instance_t *instance );

/* Init the control plane and create instance */
ut_controlPlane_instance_t *UT_ControlPlane_Init( uint32_t monitorPort );

/* pInstance, will be freed on return from the callback */
void UT_ControlPlane_RegisterCallbackOnMessage( ut_controlPlane_instance_t *pInstance, char* key, ut_control_callback_t callbackFunction);

/* Exit the controlPlane instance and release */
void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance );

typedef struct
{
  char key[UT_CONTROL_PLANE_MAX_KEY_SIZE];
  ut_control_callback_t pCallback;
}CallbackEntry_t;

//static CallbackEntry_t callbackList[UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES];
//static uint32_t lastFreeCallbackSlot=0; /* Must always be < UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES */