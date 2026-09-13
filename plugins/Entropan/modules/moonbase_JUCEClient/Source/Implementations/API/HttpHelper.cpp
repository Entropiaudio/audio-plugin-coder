#include "HttpHelper.h"

using namespace Moonbase;

//---------------------------------------------------------------------
HttpHelper::HttpHelper (juce::URL url, const juce::String& requestHeader, bool isPost, std::function<void (bool, juce::String, HttpHelper*)> onResponseCallback)
:
Thread ("HttpPostHelper"),
header (requestHeader),
inputStreamUrl (std::move (url)),
onResponse (std::move (onResponseCallback)),
post (isPost)
{
    startThread ();
}

HttpHelper::HttpHelper (juce::URL url, const juce::StringArray& requestHeaders, bool isPost, std::function<void (bool, juce::String, HttpHelper*)> onResponseCallback)
:
Thread ("HttpPostHelper"),
header (requestHeaders.joinIntoString ("\r\n")),
inputStreamUrl (std::move (url)),
onResponse (std::move (onResponseCallback)),
post (isPost)
{
    startThread ();
}

HttpHelper::HttpHelper (juce::URL url, const juce::String& requestHeader, bool isPost, std::function<void (bool, juce::MemoryBlock& responseBlock, HttpHelper*)> onResponseCallback)
:
Thread ("HttpPostHelper"),
header (requestHeader),
inputStreamUrl (std::move (url)),
onResponseBlock (std::move (onResponseCallback)),
post (isPost)
{
    startThread ();
}

HttpHelper::HttpHelper (juce::URL url, const juce::StringArray& requestHeaders, bool isPost, std::function<void (const juce::String& message, bool isStart, bool isEnd, bool isError, HttpHelper*)> onStreamingResponseCallback)
:
Thread ("HttpPostHelper"),
header (requestHeaders.joinIntoString ("\r\n")),
inputStreamUrl (std::move (url)),
isStreaming (true),
onStreamingResponse (std::move (onStreamingResponseCallback)),
post (isPost)
{
    startThread ();
}

HttpHelper::~HttpHelper ()
{
    if (isThreadRunning ())
    {
        signalThreadShouldExit ();
        stopThread ( (int)HttpTimeout * 2);
    }
}


int HttpHelper::getResponseCode () const
{
    return responseCode.get ();
}

void HttpHelper::run ()
{
    if (isStreaming)
        processStreaming ();
    else
        processBlocking ();
}

bool HttpHelper::progressCallback (int bytesSent, int totalBytes) const
{
    //return fals to cancel the request

    if (!runSync && threadShouldExit())
        return false;

    if (totalBytes == 0)
        return true;

    // const double percent = ((double)bytesSent / (double) totalBytes * 100.0);
    // std::cout << " -- " << String (percent, 1).toRawUTF8 () << "%, " << String (bytesSent).toRawUTF8 () << " of " << String (totalBytes).toRawUTF8 () << " bytes sent" << std::endl;
    juce::ignoreUnused (bytesSent);
    return true;
}

void HttpHelper::processBlocking ()
{
    requestUrl = inputStreamUrl.toString (true);

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
        .withProgressCallback (std::bind (&HttpHelper::progressCallback, this, std::placeholders::_1, std::placeholders::_2))
        .withExtraHeaders (header)
        .withConnectionTimeoutMs (HttpTimeout)
        .withResponseHeaders (&responseHeaders)
        .withStatusCode (&httpStatusCode)
    ;

    if (auto inputStream = inputStreamUrl.createInputStream (options))
    {
        if (onResponseBlock != nullptr)
        {
            if (! juce::Range<int> (200, 299).contains (httpStatusCode) )
            {
                responseCode = httpStatusCode;

                const auto responseString = inputStream->readEntireStreamAsString();
                if (responseString.isNotEmpty ())
                {
                    DBG(responseString);
                    responseBlock = juce::MemoryBlock (responseString.toRawUTF8(), static_cast<size_t> (responseString.length()));
                }
                if (!runSync)
                    juce::MessageManager::callAsync ([instance = juce::WeakReference<HttpHelper> (this)] ()
                    {
                        if (instance && instance->onResponseBlock)
                            instance->onResponseBlock (false, instance->responseBlock, instance.get());
                    });
                else
                    onResponseBlock (false, responseBlock, this);

                return;
            }
            const auto length = inputStream->getTotalLength();
            if (length < 0 )
            {
                responseCode = 500;
                if (!runSync)
                    juce::MessageManager::callAsync ([instance = juce::WeakReference<HttpHelper> (this)] ()
                    {
                        if (instance && instance->onResponseBlock)
                            instance->onResponseBlock (false, instance->responseBlock, instance.get());
                    });
                else
                    onResponseBlock (false, responseBlock, this);
                return;
            }
        
            // responseBlock.setSize (inputStream->getTotalLength());
            inputStream->readIntoMemoryBlock (responseBlock);
            responseCode = httpStatusCode;
            if (!runSync)
                juce::MessageManager::callAsync ([instance = juce::WeakReference<HttpHelper> (this)] ()
                {
                    if (instance)
                        instance->onResponseBlock (juce::Range<int> (200, 299).contains (instance->httpStatusCode), instance->responseBlock, instance.get());
                });
            else
                onResponseBlock (juce::Range<int> (200, 299).contains (httpStatusCode), responseBlock, this);
                
        }
        else
        {
            auto response = inputStream->readEntireStreamAsString();

            responseCode = httpStatusCode;

            if (!runSync)
                juce::MessageManager::callAsync ([instance = juce::WeakReference<HttpHelper> (this), response] ()
                {
                    if (instance && instance->onResponse)
                        instance->onResponse (juce::Range<int> (200, 299).contains (instance->httpStatusCode), response, instance.get());
                });
            else
                onResponse (juce::Range<int> (200, 299).contains (httpStatusCode), response, this);

        }

    }
    else
    {
        responseCode = 408;
        if (!runSync)
            juce::MessageManager::callAsync ([instance = juce::WeakReference<HttpHelper> (this)] ()
            {
                if (instance && instance->onResponse)
                    instance->onResponse (false, "Request timed out", instance.get());
            });
        else
            onResponse (false, "Request timed out", this);
    }
}

void HttpHelper::processStreaming()
{
    requestUrl = inputStreamUrl.toString (true);
    
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
        .withProgressCallback (std::bind (&HttpHelper::progressCallback, this, std::placeholders::_1, std::placeholders::_2))
        .withExtraHeaders (header)
        .withConnectionTimeoutMs (HttpTimeout)
        .withResponseHeaders (&responseHeaders)
        .withStatusCode (&httpStatusCode)
    ;

    if (auto inputStream = inputStreamUrl.createInputStream(options))
    {
        int bufferSize = 4096;
        juce::HeapBlock<char> buffer(bufferSize);
        bool isStart = true, isEnd = false;
        
        for (;;)
        {
            if (threadShouldExit())
                break;

            int bytesRead = inputStream->read(buffer, bufferSize);
            
            if (bytesRead > 0)
            {
                juce::String message (buffer, static_cast<size_t> (bytesRead));

                
                juce::MessageManager::callAsync([instance = juce::WeakReference<HttpHelper> (this), message, isStart, isEnd]()
                {
                    if (instance && instance->onStreamingResponse)
                        instance->onStreamingResponse (message, isStart, isEnd, false, instance.get());
                });

                if (isStart)
                    isStart = false;
            }
            else
            {
                if (!isStart)
                {
                    isEnd = true;
                  
                    juce::MessageManager::callAsync([instance = juce::WeakReference<HttpHelper> (this), isStart, isEnd]()
                    {
                        if (instance && instance->onStreamingResponse)
                            instance->onStreamingResponse ("", isStart, isEnd, false, instance.get());
                    });
                }
                break;
            }
        }
    }
    else
    {
        juce::MessageManager::callAsync([instance = juce::WeakReference<HttpHelper> (this)]()
        {
            if (instance && instance->onStreamingResponse)
                instance->onStreamingResponse ("Request timed out", false, false, true, instance.get());
        });
    }
}





ImageDownloader::ImageDownloader (juce::URL imageUrlToDownload, std::function<void (juce::Image, ImageDownloader*)> onImageReceivedCallback)
    : juce::Thread ("ImageDownloadWorker"),
      imageUrl (std::move (imageUrlToDownload)),
      onImageReceived (std::move (onImageReceivedCallback))
{
    startThread();
}

ImageDownloader::~ImageDownloader ()
{
    stopThread(httpTimeout);
}

void ImageDownloader::run ()
{
    juce::MemoryOutputStream memoryStream;
    juce::Image img {};

    std::unique_ptr<juce::InputStream> stream ( 
        imageUrl.createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs  (httpTimeout)
        )
    );

    if (stream != nullptr)
    {
        const int bufferSize = 8192;
        juce::HeapBlock<char> buffer (bufferSize);

        while (!stream->isExhausted() && !threadShouldExit())
        {
            auto bytesRead = stream->read (buffer.getData(), bufferSize);
            if (bytesRead <= 0)
                break;
            memoryStream.write (buffer.getData(), (size_t)bytesRead);
        }

        if (!threadShouldExit())
        {
            juce::MemoryBlock memoryBlock (memoryStream.getData(), memoryStream.getDataSize());
            img = juce::ImageFileFormat::loadFrom (memoryBlock.getData(), memoryBlock.getSize());
        }
    }

    if (!threadShouldExit())
    {
        juce::MessageManager::callAsync ([&, img, weak = juce::WeakReference (this)]()
        {
            if (weak && onImageReceived)
                onImageReceived (img, this);
        });
    }
}
