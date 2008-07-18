set( Generated_QtNetwork_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QAbstractSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QAuthenticator.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QFtp.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QHostAddress.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QHostInfo.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QHttp.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QHttpRequestHeader.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QHttpResponseHeader.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QIPv6Address.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QLocalServer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QLocalSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QNetworkAccessManager.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QNetworkAddressEntry.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QNetworkCookie.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QNetworkCookieJar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QNetworkInterface.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QNetworkProxy.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QNetworkReply.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QNetworkRequest.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QSslCertificate.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QSslCipher.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QSslConfiguration.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QSslError.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QSslKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QSslSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QTcpServer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QTcpSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QUdpSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscript_QUrlInfo.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QAbstractSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QFtp.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QHttp.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QHttpRequestHeader.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QHttpResponseHeader.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QIPv6Address.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QLocalServer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QLocalSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QNetworkAccessManager.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QNetworkCookieJar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QNetworkReply.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QSslSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QTcpServer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QTcpSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QUdpSocket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/qtscriptshell_QUrlInfo.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated_cpp/com_trolltech_qt_network/main.cpp
)
set_source_files_properties( ${Generated_QtNetwork_SRCS} PROPERTIES GENERATED true )
#qtscript bindings don't use moc
add_library( qtscript_network MODULE ${Generated_QtNetwork_SRCS} )
add_dependencies( qtscript_network generator )
target_link_libraries( qtscript_network ${QT_LIBRARIES})
install( TARGETS qtscript_network DESTINATION lib/kde4/plugins/script )