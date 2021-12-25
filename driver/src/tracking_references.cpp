#include "tracking_references.h"

#include "packets.h"

//-----------------------------------------------------------------------------
// Purpose: settings manager using a tracking reference, this is meant to be
// a settings and other utility communication and management tool
// for that purpose it will have a it's own server connection and poser protocol
// for now tho it will just stay as a tracking reference
// also a device that will always be present with hobo_vr devices from now on
// it will also allow live device management in the future
// 
// think of it as a settings manager made to look like a tracking reference
//-----------------------------------------------------------------------------


HobovrTrackingRef_SettManager::HobovrTrackingRef_SettManager(
    std::string myserial
): m_sSerialNumber(myserial) {

    m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
    m_ulPropertyContainer = vr::k_ulInvalidPropertyContainer;

    m_sModelNumber = "tracking_reference_" + m_sSerialNumber;
    m_sRenderModelPath = "{hobovr}/rendermodels/hobovr_tracking_reference";

    DriverLog(
        "%s: settings manager created",
        m_sSerialNumber
    );

    // manager stuff
    try {
        m_pSocketComm = std::make_shared<recvv::DriverReceiver>(
            sizeof(HoboVR_ManagerMsg_t),
            6969,
            "127.0.01",
            "monky\n",
            nullptr
        );
        m_pSocketComm->Start();
    } catch (const std::exception& e) {
        DriverLog(
            "%s: failed to start receiver: %s",
            m_sSerialNumber,
            e.what()
        );
    }
    m_pSocketComm->setCallback(this);

    m_Pose.poseTimeOffset = 0;
    m_Pose.result = vr::TrackingResult_Running_OK;
    m_Pose.poseIsValid = true;
    m_Pose.deviceIsConnected = true;
    m_Pose.poseTimeOffset = 0;
    m_Pose.qWorldFromDriverRotation = {1, 0, 0, 0};
    m_Pose.qDriverFromHeadRotation = {1, 0, 0, 0};
    m_Pose.qRotation = {1, 0, 0, 0};
    m_Pose.vecPosition[0] = 0.01;
    m_Pose.vecPosition[1] = 0;
    m_Pose.vecPosition[2] = 0;
    m_Pose.willDriftInYaw = false;
    m_Pose.deviceIsConnected = true;
    m_Pose.poseIsValid = true;
    m_Pose.shouldApplyHeadModel = false;
}

///////////////////////////////////////////////////////////////////////////////////

void HobovrTrackingRef_SettManager::OnPacket(char* buff, int len) {

    if (len != 523) {
        return; // do nothing if bad message
    }

    (void)buff;

    HoboVR_ManagerMsg_t* message = (HoboVR_ManagerMsg_t*)buff;

    switch (message->type) {
        case EManagerMsgType_ipd: {
            vr::VRSettings()->SetFloat(
                k_pch_Hmd_Section,
                k_pch_Hmd_IPD_Float,
                message->ipd.ipd_meters
            );

            HoboVR_ManagerResponse_t resp{EManagerResponse_ok};
            m_pSocketComm->Send(
                &resp,
                sizeof(resp)
            );
            DriverLog("tracking reference: ipd change request processed");
            break;
        }

        case EManagerMsgType_uduString: {
            std::vector<std::string> temp;
            for (int i=0; i < message->udu.len; i+=2) {
                std::string p;

                if (message->udu.devices[i].device_type == 0)
                    p = "h";
                else if (message->udu.devices[i].device_type == 1)
                    p = "c";
                else if (message->udu.devices[i].device_type == 2)
                    p = "t";
                else if (message->udu.devices[i].device_type == 3)
                    p = "g";

                temp.push_back(p);
            }
            m_vpUduChangeBuffer = temp;

            vr::VREvent_Notification_t event_data = {20, 0};
            vr::VRServerDriverHost()->VendorSpecificEvent(
                m_unObjectId,
                (vr::EVREventType)EHoboVR_VendorEvents::EHoboVR_UduChange,
                (vr::VREvent_Data_t&)event_data,
                0
            );

            DriverLog(
                "tracking reference: udu settings change request processed"
            );
            HoboVR_ManagerResponse_t resp{EManagerResponse_ok};
            m_pSocketComm->Send(
                &resp,
                sizeof(resp)
            );
            break;
        }

        case EManagerMsgType_setSelfPose: {

            m_Pose.vecPosition[0] = message->self_pose.position[0];
            m_Pose.vecPosition[1] = message->self_pose.position[1];
            m_Pose.vecPosition[2] = message->self_pose.position[2];

            if (m_unObjectId != vr::k_unTrackedDeviceIndexInvalid) {
                vr::VRServerDriverHost()->TrackedDevicePoseUpdated(
                    m_unObjectId,
                    m_Pose,
                    sizeof(m_Pose)
                );
            }

            HoboVR_ManagerResponse_t resp{EManagerResponse_ok};
            m_pSocketComm->Send(
                &resp,
                sizeof(resp)
            );
            DriverLog("tracking reference: pose change request processed");
            break;
        }

        case EManagerMsgType_distortion: {
            float newK1 = message->distortion.k1;
            float newK2 = message->distortion.k2;
            float newZoomW = message->distortion.zoom_width;
            float newZoomH = message->distortion.zoom_height;

            vr::VRSettings()->SetFloat(
                hobovr::k_pch_ExtDisplay_Section,
                hobovr::k_pch_ExtDisplay_DistortionK1_Float,
                newK1
            );

            vr::VRSettings()->SetFloat(
                hobovr::k_pch_ExtDisplay_Section,
                hobovr::k_pch_ExtDisplay_DistortionK2_Float,
                newK2
            );

            vr::VRSettings()->SetFloat(
                hobovr::k_pch_ExtDisplay_Section,
                hobovr::k_pch_ExtDisplay_ZoomWidth_Float,
                newZoomW
            );

            vr::VRSettings()->SetFloat(
                hobovr::k_pch_ExtDisplay_Section,
                hobovr::k_pch_ExtDisplay_ZoomHeight_Float,
                newZoomH
            );


            HoboVR_ManagerResponse_t resp{EManagerResponse_ok};
            m_pSocketComm->Send(
                &resp,
                sizeof(resp)
            );
            DriverLog("tracking reference: distortion update request processed");
            break;
        }

        case EManagerMsgType_eyeGap: {
            vr::VRSettings()->SetInt32(
                hobovr::k_pch_ExtDisplay_Section,
                hobovr::k_pch_ExtDisplay_EyeGapOffset_Int,
                message->eye_offset.width
            );

            HoboVR_ManagerResponse_t resp{EManagerResponse_ok};
            m_pSocketComm->Send(
                &resp,
                sizeof(resp)
            );
            DriverLog("tracking reference: eye gap change request processed");
            break;
        }

        case EManagerMsgType_poseTimeOffset: {
            float newTimeOffset = message->time_offset.time_offset_seconds;

            vr::VRSettings()->SetFloat(
                k_pch_Hobovr_Section,
                hobovr::k_pch_Hobovr_PoseTimeOffset_Float,
                newTimeOffset
            );


            HoboVR_ManagerResponse_t resp{EManagerResponse_ok};
            m_pSocketComm->Send(
                &resp,
                sizeof(resp)
            );
            DriverLog(
                "tracking reference: pose time offset change request processed"
            );
            break;
        }

        default:
            DriverLog("tracking reference: message not recognized");

            HoboVR_ManagerResponse_t resp{EManagerResponse_invalid};
            m_pSocketComm->Send(
                &resp,
                sizeof(resp)
            );
    }
}

///////////////////////////////////////////////////////////////////////////////////

vr::EVRInitError HobovrTrackingRef_SettManager::Activate(
    vr::TrackedDeviceIndex_t unObjectId
) {
    m_unObjectId = unObjectId;
    m_ulPropertyContainer =
        vr::VRProperties()->TrackedDeviceToPropertyContainer(
            m_unObjectId
        );

    vr::VRProperties()->SetStringProperty(
        m_ulPropertyContainer,
        vr::Prop_ModelNumber_String,
        m_sModelNumber.c_str()
    );

    vr::VRProperties()->SetStringProperty(
        m_ulPropertyContainer,
        vr::Prop_RenderModelName_String,
        m_sRenderModelPath.c_str()
    );

    DriverLog("device: tracking reference activated\n");
    DriverLog("device: \tserial: %s\n", m_sSerialNumber.c_str());
    DriverLog("device: \tmodel: %s\n", m_sModelNumber.c_str());
    DriverLog("device: \trender model path: \"%s\"\n", m_sRenderModelPath.c_str());

    // return a constant that's not 0 (invalid) or 1 (reserved for Oculus)
    vr::VRProperties()->SetUint64Property(
        m_ulPropertyContainer,
        vr::Prop_CurrentUniverseId_Uint64,
        2
    );

    vr::VRProperties()->SetFloatProperty(
        m_ulPropertyContainer,
        vr::Prop_FieldOfViewLeftDegrees_Float,
        180
    );

    vr::VRProperties()->SetFloatProperty(
        m_ulPropertyContainer,
        vr::Prop_FieldOfViewRightDegrees_Float,
        180
    );

    vr::VRProperties()->SetFloatProperty(
        m_ulPropertyContainer,
        vr::Prop_FieldOfViewTopDegrees_Float,
        180
    );

    vr::VRProperties()->SetFloatProperty(
        m_ulPropertyContainer,
        vr::Prop_FieldOfViewBottomDegrees_Float,
        180
    );

    vr::VRProperties()->SetFloatProperty(
        m_ulPropertyContainer,
        vr::Prop_TrackingRangeMinimumMeters_Float,
        0
    );

    vr::VRProperties()->SetFloatProperty(
        m_ulPropertyContainer,
        vr::Prop_TrackingRangeMaximumMeters_Float,
        10
    );

    vr::VRProperties()->SetStringProperty(
        m_ulPropertyContainer,
        vr::Prop_ModeLabel_String,
        m_sModelNumber.c_str()
    );

    vr::VRProperties()->SetBoolProperty(
        m_ulPropertyContainer,
        vr::Prop_CanWirelessIdentify_Bool,
        false
    );

    vr::DriverPose_t pose;
    pose.poseTimeOffset = 0;
    pose.result = vr::TrackingResult_Running_OK;
    pose.poseIsValid = true;
    pose.deviceIsConnected = true;
    pose.poseTimeOffset = 0;
    pose.qWorldFromDriverRotation = {1, 0, 0, 0};
    pose.qDriverFromHeadRotation = {1, 0, 0, 0};
    pose.qRotation = {1, 0, 0, 0};
    pose.vecPosition[0] = 0.01;
    pose.vecPosition[1] = 0;
    pose.vecPosition[2] = 0;
    pose.willDriftInYaw = false;
    pose.deviceIsConnected = true;
    pose.poseIsValid = true;
    pose.shouldApplyHeadModel = false;

    pose.vecPosition[0] = 0;
    pose.vecPosition[1] = 0;
    pose.vecPosition[2] = 0;

    if (m_unObjectId != vr::k_unTrackedDeviceIndexInvalid) {
        vr::VRServerDriverHost()->TrackedDevicePoseUpdated(
            m_unObjectId,
            pose,
            sizeof(pose)
        );
    }

    return vr::VRInitError_None;
}

///////////////////////////////////////////////////////////////////////////////////

void HobovrTrackingRef_SettManager::Deactivate() {
    DriverLog("device: \"%s\" deactivated\n", m_sSerialNumber.c_str());
    // "signal" device disconnected
    m_Pose.poseIsValid = false;
    m_Pose.deviceIsConnected = false;

    if (m_unObjectId != vr::k_unTrackedDeviceIndexInvalid) {
        vr::VRServerDriverHost()->TrackedDevicePoseUpdated(
            m_unObjectId,
            m_Pose,
            sizeof(m_Pose)
        );
    }

    m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
}

///////////////////////////////////////////////////////////////////////////////////

void* HobovrTrackingRef_SettManager::GetComponent(
    const char* pchComponentNameAndVersion
) {
    DriverLog("tracking reference: request for \"%s\" component ignored\n",
        pchComponentNameAndVersion
    );

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////

void HobovrTrackingRef_SettManager::DebugRequest(
    const char *pchRequest,
    char *pchResponseBuffer,
    uint32_t unResponseBufferSize
) {

    DriverLog("device: \"%s\" got a debug request: \"%s\"",
        m_sSerialNumber.c_str(),
        pchRequest
    );

    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

///////////////////////////////////////////////////////////////////////////////////

vr::DriverPose_t HobovrTrackingRef_SettManager::GetPose() {
    return m_Pose;
}

///////////////////////////////////////////////////////////////////////////////////

std::string HobovrTrackingRef_SettManager::GetSerialNumber() const {
    return m_sSerialNumber;
}

///////////////////////////////////////////////////////////////////////////////////

void HobovrTrackingRef_SettManager::UpdatePose() {
    if (m_unObjectId != vr::k_unTrackedDeviceIndexInvalid) {
        DriverLog("tracking reference: pose updated");
        vr::VRServerDriverHost()->TrackedDevicePoseUpdated(
            m_unObjectId,
            m_Pose,
            sizeof(m_Pose)
        );
    }
}