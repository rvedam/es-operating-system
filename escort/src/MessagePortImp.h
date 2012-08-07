// Generated by esidl (r1752).
// This file is expected to be modified for the Web IDL interface
// implementation.  Permission to use, copy, modify and distribute
// this file in any software license is hereby granted.

#ifndef ORG_W3C_DOM_BOOTSTRAP_MESSAGEPORTIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_MESSAGEPORTIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/html/MessagePort.h>

#include <org/w3c/dom/events/Event.h>
#include <org/w3c/dom/events/EventListener.h>
#include <org/w3c/dom/html/Transferable.h>
#include <org/w3c/dom/html/Function.h>

namespace org
{
namespace w3c
{
namespace dom
{
namespace bootstrap
{
class MessagePortImp : public ObjectMixin<MessagePortImp>
{
public:
    // MessagePort
    void postMessage(Any message);
    void postMessage(Any message, Sequence<html::Transferable> transfer);
    void start();
    void close();
    html::Function getOnmessage();
    void setOnmessage(html::Function onmessage);
    // EventTarget
    void addEventListener(std::u16string type, events::EventListener listener);
    void addEventListener(std::u16string type, events::EventListener listener, bool capture);
    void removeEventListener(std::u16string type, events::EventListener listener);
    void removeEventListener(std::u16string type, events::EventListener listener, bool capture);
    bool dispatchEvent(events::Event event);
    // Transferable
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return html::MessagePort::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return html::MessagePort::getMetaData();
    }
};

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_MESSAGEPORTIMP_H_INCLUDED