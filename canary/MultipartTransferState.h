/*
 * Copyright 2010-2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#pragma once

#include <atomic>
#include <aws/common/mutex.h>
#include <aws/common/string.h>
#include <aws/crt/DateTime.h>
#include <aws/crt/Types.h>
#include <aws/crt/http/HttpConnection.h>
#include <mutex>

#include "MetricsPublisher.h"
#include "TransferState.h"

class S3ObjectTransport;
class CanaryApp;

enum class PartFinishResponse
{
    Done,
    Retry
};

/*
 * Represents a multipart object transfer state.  While it does not explicitly track an array of transfer states
 * at the moment, it does represent the progress of a number of transfer states being processed.
 */
class MultipartTransferState
{
  public:
    using PartFinishedCallback = std::function<void(PartFinishResponse response)>;
    using ProcessPartCallback =
        std::function<void(const std::shared_ptr<TransferState> &transferState, PartFinishedCallback callback)>;
    using FinishedCallback = std::function<void(int32_t errorCode)>;

    MultipartTransferState(const Aws::Crt::String &key, uint64_t objectSize, uint32_t numParts);

    virtual ~MultipartTransferState();

    bool IsFinished() const;
    const Aws::Crt::String &GetKey() const;
    uint32_t GetNumParts() const;
    uint32_t GetNumPartsCompleted() const;
    uint64_t GetObjectSize() const;

    /*
     * Callback that will be used to process a part, ie, upload or downlad it.
     */
    void SetProcessPartCallback(const ProcessPartCallback &processPartCallback);

    /*
     * Callback that will be triggered when all parts have finished processing.
     */
    void SetFinishedCallback(const FinishedCallback &finishedCallback);

    /*
     * Sets the finished status of the entire transfer, in failure or success.
     */
    void SetFinished(int32_t errorCode = AWS_ERROR_SUCCESS);

    /*
     * Increment thread safe counter that a part has completed.
     */
    bool IncNumPartsCompleted();

    /*
     * Used to invoke the internal process part callback.
     */
    template <typename... TArgs> void ProcessPart(TArgs &&... Args) const
    {
        m_processPartCallback(std::forward<TArgs>(Args)...);
    }

  private:
    int32_t m_errorCode;
    uint32_t m_numParts;
    std::atomic<bool> m_isFinished;
    std::atomic<uint32_t> m_numPartsCompleted;
    uint64_t m_objectSize;
    Aws::Crt::String m_key;
    ProcessPartCallback m_processPartCallback;
    FinishedCallback m_finishedCallback;
};

/*
 * Represents a multipart upload transfer state.
 */
class MultipartUploadState : public MultipartTransferState
{
  public:
    MultipartUploadState(const Aws::Crt::String &key, uint64_t objectSize, uint32_t numParts);

    void SetUploadId(const Aws::Crt::String &uploadId);
    void SetETag(uint32_t partIndex, const Aws::Crt::String &etag);

    void GetETags(Aws::Crt::Vector<Aws::Crt::String> &outETags);
    const Aws::Crt::String &GetUploadId() const;

  private:
    Aws::Crt::Vector<Aws::Crt::String> m_etags;
    std::mutex m_etagsMutex;
    Aws::Crt::String m_uploadId;
};

/*
 * Represents a multipart download transfer state.
 */
class MultipartDownloadState : public MultipartTransferState
{
  public:
    MultipartDownloadState(const Aws::Crt::String &key, uint64_t objectSize, uint32_t numParts);
};
