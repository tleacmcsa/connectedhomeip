/*
 *
 *    Copyright (c) 2024 Project CHIP Authors
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

#include "AccessRestriction.h"

#include <algorithm>
#include <lib/core/Global.h>

namespace chip {
namespace Access {

std::vector<SharedPtr<AccessRestriction::Entry>> AccessRestriction::sCommissioningEntries;

CHIP_ERROR AccessRestriction::CreateFabricEntries(const FabricIndex fabricIndex)
{
    for (auto & entry : sCommissioningEntries)
    {
        CreateEntry(nullptr, *entry, fabricIndex);
    }

    return CHIP_NO_ERROR;
}

void AccessRestriction::AddListener(EntryListener & listener)
{
    if (mListeners == nullptr)
    {
        mListeners     = &listener;
        listener.mNext = nullptr;
        return;
    }

    for (EntryListener * l = mListeners; /**/; l = l->mNext)
    {
        if (l == &listener)
        {
            return;
        }

        if (l->mNext == nullptr)
        {
            l->mNext       = &listener;
            listener.mNext = nullptr;
            return;
        }
    }
}

void AccessRestriction::RemoveListener(EntryListener & listener)
{
    if (mListeners == &listener)
    {
        mListeners     = listener.mNext;
        listener.mNext = nullptr;
        return;
    }

    for (EntryListener * l = mListeners; l != nullptr; l = l->mNext)
    {
        if (l->mNext == &listener)
        {
            l->mNext       = listener.mNext;
            listener.mNext = nullptr;
            return;
        }
    }
}

SharedPtr<AccessRestriction::Entry> AccessRestriction::GetEntry(FabricIndex fabricIndex, size_t index)
{
    if (mFabricEntries.find(fabricIndex) != mFabricEntries.end() && index < mFabricEntries[fabricIndex].size())
    {
        return mFabricEntries[fabricIndex][index];
    }

    return nullptr;
}

CHIP_ERROR AccessRestriction::CreateCommissioningEntry(SharedPtr<Entry> entry)
{
    sCommissioningEntries.push_back(entry);
    return CHIP_NO_ERROR;
}

CHIP_ERROR AccessRestriction::CreateEntry(size_t * index, const Entry & entry, FabricIndex fabricIndex)
{
    auto localEntry = MakeShared<Entry>(entry);
    size_t newIndex;

    localEntry->fabricIndex = fabricIndex;

    if (mFabricEntries.find(fabricIndex) == mFabricEntries.end())
    {
        mFabricEntries[fabricIndex] = std::vector<SharedPtr<Entry>>();
        mFabricEntries[fabricIndex].push_back(localEntry);
    }
    else
    {
        mFabricEntries[fabricIndex].push_back(localEntry);
    }

    newIndex = mFabricEntries[fabricIndex].size() - 1;

    for (EntryListener * listener = mListeners; listener != nullptr; listener = listener->mNext)
    {
        listener->OnEntryChanged(fabricIndex, newIndex, localEntry, EntryListener::ChangeType::kAdded);
    }

    if (index != nullptr)
    {
        *index = newIndex;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR AccessRestriction::DeleteEntry(size_t index, const FabricIndex fabricIndex)
{
    auto entry = GetEntry(fabricIndex, index);
    if (entry == nullptr)
    {
        return CHIP_ERROR_NOT_FOUND;
    }
    else
    {
        mFabricEntries[fabricIndex].erase(mFabricEntries[fabricIndex].begin() + index);

        for (EntryListener * listener = mListeners; listener != nullptr; listener = listener->mNext)
        {
            listener->OnEntryChanged(fabricIndex, index, entry, EntryListener::ChangeType::kRemoved);
        }
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR AccessRestriction::UpdateEntry(size_t index, const Entry & entry, const FabricIndex fabricIndex)
{
    auto localEntry = GetEntry(fabricIndex, index);
    if (localEntry != nullptr)
    {
        *localEntry = entry;

        for (EntryListener * listener = mListeners; listener != nullptr; listener = listener->mNext)
        {
            listener->OnEntryChanged(fabricIndex, index, localEntry, EntryListener::ChangeType::kUpdated);
        }

        return CHIP_NO_ERROR;
    }

    return CHIP_ERROR_NOT_FOUND;
}

CHIP_ERROR AccessRestriction::Check(const SubjectDescriptor & subjectDescriptor, const RequestPath & requestPath)
{
    ChipLogProgress(DataManagement, "AccessRestriction: action %d", static_cast<int>(requestPath.requestType));

    if (requestPath.requestType == RequestType::kRequestTypeUnknown)
    {
        ChipLogError(DataManagement, "AccessRestriction: RequestPath type is unknown");
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    //wildcard event subscriptions are allowed.
    if (requestPath.requestType == RequestType::kEventReadOrSubscribeRequest && !requestPath.entityId.has_value())
    {
        return CHIP_NO_ERROR;
    } 

    // TODO this lookup needs to be optimized
    for (auto & entry : mFabricEntries[subjectDescriptor.fabricIndex])
    {
        if (entry->endpointNumber != requestPath.endpoint || entry->clusterId != requestPath.cluster)
        {
            continue;
        }

        for (auto & restriction : entry->restrictions)
        {
            // a missing id is a wildcard
            bool idMatch = !restriction.id.HasValue() || restriction.id.Value() == requestPath.entityId;
            if (!idMatch)
            {
                continue;
            }

            switch (restriction.restrictionType)
            {
            case Type::kAttributeAccessForbidden:
                if (requestPath.requestType == RequestType::kAttributeReadRequest ||
                    requestPath.requestType == RequestType::kAttributeWriteRequest)
                {
                    return CHIP_ERROR_ACCESS_DENIED;
                }
                break;
            case Type::kAttributeWriteForbidden:
                if (requestPath.requestType == RequestType::kAttributeWriteRequest)
                {
                    return CHIP_ERROR_ACCESS_DENIED;
                }
                break;
            case Type::kCommandForbidden:
                if (requestPath.requestType == RequestType::kCommandInvokeRequest)
                {
                    return CHIP_ERROR_ACCESS_DENIED;
                }
                break;
            case Type::kEventForbidden:
                if (requestPath.requestType == RequestType::kEventReadOrSubscribeRequest)
                {
                    return CHIP_ERROR_ACCESS_DENIED;
                }
                break;
            }
        }
    }

    return CHIP_NO_ERROR;
}

} // namespace Access
} // namespace chip