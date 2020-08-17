
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/qxmpp)
    add_subdirectory(qxmpp)
else()
    find_package(QXmpp 1.0 QUIET CONFIG)
endif()

if(TARGET QXmpp::QXmpp)
    set(xmpp_SOURCES
        TelegramXmppServerExtension.cpp
        TelegramXmppServerExtension.hpp
        XmppFederation.cpp
        XmppFederation.hpp
        XmppUser.cpp
        XmppUser.hpp
    )

    add_library(XmppFederalization STATIC
        ${xmpp_SOURCES}
    )
    target_link_libraries(XmppFederalization PUBLIC
        Qt5::Core
        Qt5::Network
        QXmpp::QXmpp
        TelegramQt${QT_VERSION_MAJOR}::Server
    )

    target_include_directories(XmppFederalization PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    add_library(Internal::XmppPlugin ALIAS XmppFederalization)

    add_library(XmppPlugin STATIC XmppPlugin.cpp)

    target_link_libraries(XmppPlugin
        Qt5::Core
        TelegramQt${QT_VERSION_MAJOR}::Server
    )

    #install(
    #    TARGETS XmppPlugin
    #    DESTINATION ${INSTALL_QML_IMPORT_DIR}/TelegramQt
    #    COMPONENT Runtime
    #)
endif()
