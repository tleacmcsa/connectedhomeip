/*
 *
 *    Copyright (c) 2022-2023 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <app/clusters/door-lock-server/door-lock-delegate.h>
#include <app/clusters/door-lock-server/door-lock-server.h>
#include <crypto/CHIPCryptoPAL.h>
#include <vector>

struct LockUserInfo
{
    char userName[DOOR_LOCK_USER_NAME_BUFFER_SIZE];
    uint32_t userUniqueId;
    UserStatusEnum userStatus;
    UserTypeEnum userType;
    CredentialRuleEnum credentialRule;
    std::vector<CredentialStruct> credentials;
    chip::FabricIndex createdBy;
    chip::FabricIndex lastModifiedBy;
};

struct LockCredentialInfo;
struct WeekDaysScheduleInfo;
struct YearDayScheduleInfo;
struct HolidayScheduleInfo;

static constexpr size_t DOOR_LOCK_CREDENTIAL_INFO_MAX_DATA_SIZE = 65;
static constexpr size_t DOOR_LOCK_CREDENTIAL_INFO_MAX_TYPES     = 9;

class LockEndpoint : public chip::app::Clusters::DoorLock::Delegate
{
public:
    LockEndpoint(chip::EndpointId endpointId, uint16_t numberOfLockUsersSupported, uint16_t numberOfCredentialsSupported,
                 uint8_t weekDaySchedulesPerUser, uint8_t yearDaySchedulesPerUser, uint8_t numberOfCredentialsPerUser,
                 uint8_t numberOfHolidaySchedules) :
        mEndpointId{ endpointId },
        mLockState{ DlLockState::kLocked }, mDoorState{ DoorStateEnum::kDoorClosed }, mLockUsers(numberOfLockUsersSupported),
        mLockCredentials(DOOR_LOCK_CREDENTIAL_INFO_MAX_TYPES, std::vector<LockCredentialInfo>(numberOfCredentialsSupported + 1)),
        mWeekDaySchedules(numberOfLockUsersSupported, std::vector<WeekDaysScheduleInfo>(weekDaySchedulesPerUser)),
        mYearDaySchedules(numberOfLockUsersSupported, std::vector<YearDayScheduleInfo>(yearDaySchedulesPerUser)),
        mHolidaySchedules(numberOfHolidaySchedules)
    {
        for (auto & lockUser : mLockUsers)
        {
            lockUser.credentials.reserve(numberOfCredentialsPerUser);
        }
        DoorLockServer::Instance().SetDoorState(endpointId, mDoorState);
        DoorLockServer::Instance().SetLockState(endpointId, mLockState);
        chip::Crypto::DRBG_get_bytes(mAliroReaderGroupSubIdentifier, sizeof(mAliroReaderGroupSubIdentifier));
    }

    inline chip::EndpointId GetEndpointId() const { return mEndpointId; }

    bool Lock(const Nullable<chip::FabricIndex> & fabricIdx, const Nullable<chip::NodeId> & nodeId,
              const Optional<chip::ByteSpan> & pin, OperationErrorEnum & err, OperationSourceEnum opSource);
    bool Unlock(const Nullable<chip::FabricIndex> & fabricIdx, const Nullable<chip::NodeId> & nodeId,
                const Optional<chip::ByteSpan> & pin, OperationErrorEnum & err, OperationSourceEnum opSource);
    bool Unbolt(const Nullable<chip::FabricIndex> & fabricIdx, const Nullable<chip::NodeId> & nodeId,
                const Optional<chip::ByteSpan> & pin, OperationErrorEnum & err, OperationSourceEnum opSource);

    bool GetUser(uint16_t userIndex, EmberAfPluginDoorLockUserInfo & user) const;
    bool SetUser(uint16_t userIndex, chip::FabricIndex creator, chip::FabricIndex modifier, const chip::CharSpan & userName,
                 uint32_t uniqueId, UserStatusEnum userStatus, UserTypeEnum usertype, CredentialRuleEnum credentialRule,
                 const CredentialStruct * credentials, size_t totalCredentials);

    bool SetDoorState(DoorStateEnum newState);

    DoorStateEnum GetDoorState() const;

    bool SendLockAlarm(AlarmCodeEnum alarmCode) const;

    bool GetCredential(uint16_t credentialIndex, CredentialTypeEnum credentialType,
                       EmberAfPluginDoorLockCredentialInfo & credential) const;

    bool SetCredential(uint16_t credentialIndex, chip::FabricIndex creator, chip::FabricIndex modifier,
                       DlCredentialStatus credentialStatus, CredentialTypeEnum credentialType,
                       const chip::ByteSpan & credentialData);

    DlStatus GetSchedule(uint8_t weekDayIndex, uint16_t userIndex, EmberAfPluginDoorLockWeekDaySchedule & schedule);
    DlStatus GetSchedule(uint8_t yearDayIndex, uint16_t userIndex, EmberAfPluginDoorLockYearDaySchedule & schedule);
    DlStatus GetSchedule(uint8_t holidayIndex, EmberAfPluginDoorLockHolidaySchedule & schedule);

    DlStatus SetSchedule(uint8_t weekDayIndex, uint16_t userIndex, DlScheduleStatus status, DaysMaskMap daysMask, uint8_t startHour,
                         uint8_t startMinute, uint8_t endHour, uint8_t endMinute);
    DlStatus SetSchedule(uint8_t yearDayIndex, uint16_t userIndex, DlScheduleStatus status, uint32_t localStartTime,
                         uint32_t localEndTime);
    DlStatus SetSchedule(uint8_t holidayIndex, DlScheduleStatus status, uint32_t localStartTime, uint32_t localEndTime,
                         OperatingModeEnum operatingMode);

    // DoorLock::Delegate API.
    CHIP_ERROR GetAliroReaderVerificationKey(chip::MutableByteSpan & verificationKey) override;
    CHIP_ERROR GetAliroReaderGroupIdentifier(chip::MutableByteSpan & groupIdentifier) override;
    CHIP_ERROR GetAliroReaderGroupSubIdentifier(chip::MutableByteSpan & groupSubIdentifier) override;
    CHIP_ERROR GetAliroExpeditedTransactionSupportedProtocolVersionAtIndex(size_t index,
                                                                           chip::MutableByteSpan & protocolVersion) override;
    CHIP_ERROR GetAliroGroupResolvingKey(chip::MutableByteSpan & groupResolvingKey) override;
    CHIP_ERROR GetAliroSupportedBLEUWBProtocolVersionAtIndex(size_t index, chip::MutableByteSpan & protocolVersion) override;
    uint8_t GetAliroBLEAdvertisingVersion() override;
    uint16_t GetNumberOfAliroCredentialIssuerKeysSupported() override;
    uint16_t GetNumberOfAliroEndpointKeysSupported() override;
    CHIP_ERROR SetAliroReaderConfig(const chip::ByteSpan & signingKey, const chip::ByteSpan & verificationKey,
                                    const chip::ByteSpan & groupIdentifier,
                                    const chip::Optional<chip::ByteSpan> & groupResolvingKey) override;
    CHIP_ERROR ClearAliroReaderConfig() override;

private:
    bool setLockState(const Nullable<chip::FabricIndex> & fabricIdx, const Nullable<chip::NodeId> & nodeId, DlLockState lockState,
                      const Optional<chip::ByteSpan> & pin, OperationErrorEnum & err,
                      OperationSourceEnum opSource = OperationSourceEnum::kUnspecified);
    const char * lockStateToString(DlLockState lockState) const;

    // Returns true if week day schedules should apply to the user, there are
    // schedules defined for the user, and access is not currently allowed by
    // those schedules.  The outparam indicates whether there were in fact any
    // year day schedules defined for the user.
    bool weekDayScheduleForbidsAccess(uint16_t userIndex, bool * haveSchedule) const;
    // Returns true if year day schedules should apply to the user, there are
    // schedules defined for the user, and access is not currently allowed by
    // those schedules.  The outparam indicates whether there were in fact any
    // year day schedules defined for the user.
    bool yearDayScheduleForbidsAccess(uint16_t userIndex, bool * haveSchedule) const;

    static void OnLockActionCompleteCallback(chip::System::Layer *, void * callbackContext);

    chip::EndpointId mEndpointId;
    DlLockState mLockState;
    DoorStateEnum mDoorState;

    // This is very naive implementation of users/credentials/schedules database and by no means the best practice. Proper storage
    // of those items is out of scope of this example.
    std::vector<LockUserInfo> mLockUsers;
    std::vector<std::vector<LockCredentialInfo>> mLockCredentials;
    std::vector<std::vector<WeekDaysScheduleInfo>> mWeekDaySchedules;
    std::vector<std::vector<YearDayScheduleInfo>> mYearDaySchedules;
    std::vector<HolidayScheduleInfo> mHolidaySchedules;

    // Actual Aliro state would presumably be stored somewhere else, and persistently; this
    // example just stores it in memory for illustration purposes.
    uint8_t mAliroReaderVerificationKey[chip::app::Clusters::DoorLock::kAliroReaderVerificationKeySize];
    uint8_t mAliroReaderGroupIdentifier[chip::app::Clusters::DoorLock::kAliroReaderGroupIdentifierSize];
    uint8_t mAliroReaderGroupSubIdentifier[chip::app::Clusters::DoorLock::kAliroReaderGroupSubIdentifierSize];
    uint8_t mAliroGroupResolvingKey[chip::app::Clusters::DoorLock::kAliroGroupResolvingKeySize];
    bool mAliroStateInitialized = false;
};

struct LockCredentialInfo
{
    DlCredentialStatus status;
    CredentialTypeEnum credentialType;
    chip::FabricIndex createdBy;
    chip::FabricIndex modifiedBy;
    uint8_t credentialData[DOOR_LOCK_CREDENTIAL_INFO_MAX_DATA_SIZE];
    size_t credentialDataSize;
};

struct WeekDaysScheduleInfo
{
    DlScheduleStatus status;
    EmberAfPluginDoorLockWeekDaySchedule schedule;
};

struct YearDayScheduleInfo
{
    DlScheduleStatus status;
    EmberAfPluginDoorLockYearDaySchedule schedule;
};

struct HolidayScheduleInfo
{
    DlScheduleStatus status;
    EmberAfPluginDoorLockHolidaySchedule schedule;
};
