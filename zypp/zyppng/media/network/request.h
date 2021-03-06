#ifndef ZYPP_NG_MEDIA_CURL_REQUEST_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_REQUEST_H_INCLUDED

#include <zypp/zyppng/media/network/networkrequesterror.h>
#include <zypp/zyppng/media/network/TransferSettings>
#include <zypp/zyppng/base/Base>
#include <zypp/zyppng/core/Url>
#include <zypp/zyppng/base/zyppglobal.h>
#include <zypp/zyppng/base/signals.h>
#include <zypp/base/Flags.h>
#include <vector>

namespace zypp {
  class Digest;

  namespace media {
    class AuthData;
  }
}

namespace zyppng {

  using AuthData = zypp::media::AuthData;

  class NetworkRequestDispatcher;
  class NetworkRequestPrivate;

  /*!
   * Represents a (http/https/ftp) request. This is the low level API for the
   * \sa zyppng::Downloader , usually it makes more sense to use the Downloader for the
   * more features it supports.
   * After creating a NetworkRequest and changing the required settings is enqueued
   * to the \sa zyppng::NetworkRequestDispatcher.
   */
  class LIBZYPP_NG_EXPORT NetworkRequest : public Base
  {
  public:

    using Ptr = std::shared_ptr<NetworkRequest>;
    using WeakPtr = std::weak_ptr<NetworkRequest>;

    enum State {
      Pending,    //< waiting to be dispatched
      Running,    //< currently running
      Finished,   //< finished successfully
      Error,      //< Error, use error function to figure out the issue
    };

    enum Priority {
      Normal, //< Requests with normal priority will be enqueued at the end
      High    //< Request with high priority will be moved to the front of the queue
    };

    enum FileMode {
      WriteExclusive, //< the request will create its own file, overwriting anything that already exists
      WriteShared     //< the request will create or open the file in shared mode and only write between \a start and \a len
    };

    enum OptionBits {
      HeadRequest = 0x01 //< only request the header part of the file
    };
    ZYPP_DECLARE_FLAGS(Options, OptionBits);

    /*!
     * \param url The source URL of the download
     * \param targetFile The path where the file should be stored
     * \param start File offset, if set this will create a range download request
     * \param len File range length
     * \param fMode The mode in which the file is opened in.
     */
    NetworkRequest( Url url, zypp::Pathname targetFile, off_t start = -1, off_t len = 0, FileMode fMode = WriteExclusive );
    virtual ~NetworkRequest();

    /*!
     * Sets the priority of the NetworkRequest, this will affect where
     * the \sa NetworkRequestDispatcher puts the Request in the Queue.
     * \note changing this makes only sense before enqueueing the request
     */
    void setPriority ( Priority prio );

    /*!
     * Returns the requested priority of the NetworkRequest
     */
    Priority priority ( ) const;

    /*!
     * Change request options, currently only the \sa OptionBits::HeadRequest option is supported
     *
     * \note changing this makes only sense before the request was started
     */
    void setOptions ( Options opt );

    /*!
     * Returns the currently set options
     */
    Options options () const;

    /*!
     * Sets the range description
     * \note This will not change a running download
     * \param start
     * \param len
     */
    void setRequestRange ( off_t start = -1, off_t len = 0 );

    /*!
     * Returns the last redirect information from the headers.
     */
    const std::string &lastRedirectInfo() const;

    /*!
     * Returns a pointer to the native CURL easy handle
     *
     * \note consider adding the functionality here instead of using the Handle directly.
     *       In case we ever decide to switch out the CURL backend your code will break
     */
    void *nativeHandle () const;

    /*!
     * Will return the data at \a offset with length \a count.
     * If there is not yet enough data a empty vector will be returned
     */
    std::vector<char> peekData ( off_t offset, size_t count ) const;

    /*!
     * Returns the request URL
     */
    Url url () const;

    /**
     * This will change the URL of the request.
     * \note calling this on a currently running request has no impact
     */
    void setUrl ( const Url & url );

    /**
     * Returns the target filename path
     */
    const zypp::Pathname & targetFilePath () const;

    /**
     * Returns the content type as reported from the server
     * \note can only return a valid value if the download has started already
     */
    std::string contentType () const;

    /**
     * Returns the requested start offset
     */
    off_t downloadOffset () const;

    /**
     * Returns the number of bytes that are reported from the backend as the full download size, those can
     * be 0 even when the download is already running.
     */
    off_t reportedByteCount   () const;

    /**
     * Returns the expected byte count that was passed to the constructor, zero if
     * none was given
     */
    off_t expectedByteCount   () const;

    /**
     * Returns the number of already downloaded bytes as reported by the backend
     */
    off_t downloadedByteCount () const;

    /**
     * Set a \sa zypp::Digest that is updated when data is written, can be used to
     * validate the file contents with a checksum
     */
    void setDigest ( std::shared_ptr<zypp::Digest> dig );

    /**
     * Returns the currently used \sa zypp::Digest, null if non was set.
     */
    std::shared_ptr<zypp::Digest> digest () const;

    /**
     * Enables automated checking of downloaded contents against a checksum.
     * Only makes a difference if a \sa zypp::Digest was with with \sa setDigest.
     *
     * \note expects checksum in byte NOT in string format
     */
    void setExpectedChecksum (std::vector<unsigned char> checksum );


    /*!
     * Returns a writeable reference to the internal \sa zyppng::TransferSettings.
     * \note calling this on a already running request has no impact
     */
    TransferSettings &transferSettings ();

    /**
     * Returns the current state the \a HttpDownloadRequest is in
     */
    State state () const;

    /**
     * Returns the last set Error
     */
    const NetworkRequestError &error () const;

    /**
     * In some cases curl can provide extended error informations collected at
     * runtime. In those cases its possible to query that information.
     */
    std::string extendedErrorString() const;

    /**
     * Checks if there was a error with the request
     */
    bool hasError () const;

    /*!
     * Adds a raw header to the request data. Use this to send custom headers
     * to the server.
     */
    bool addRequestHeader(const std::string &header );

    /**
     * Signals that the dispatcher dequeued the request and actually starts downloading data
     */
    SignalProxy<void ( NetworkRequest &req )> sigStarted  ();

    /**
     * Signals if there was data read from the download
     */
    SignalProxy<void ( NetworkRequest &req, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow )> sigProgress ();

    /**
     * Signals that the download finished.
     *
     * \note After this signal was emitted the Curl handle is reset,
     *       so all queries to it need to be done in a Slot connected to this signal
     */
    SignalProxy<void ( NetworkRequest &req, const NetworkRequestError &err)> sigFinished ( );

  private:
    friend class NetworkRequestDispatcher;
    friend class NetworkRequestDispatcherPrivate;
    ZYPP_DECLARE_PRIVATE( NetworkRequest )
  };

}
ZYPP_DECLARE_OPERATORS_FOR_FLAGS(zyppng::NetworkRequest::Options);

#endif
