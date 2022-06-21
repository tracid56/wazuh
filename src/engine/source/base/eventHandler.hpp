/* Copyright (C) 2015-2021, Wazuh Inc.
 * All rights reserved.
 *
 */

#ifndef _H_EVENT_HANDLER
#define _H_EVENT_HANDLER

#include <json.hpp>

namespace base
{

class EventHandler
{

private:
    // Control
    bool m_isDecoded; ///< True if it reached the end of the decoding stage
    // Data
    std::shared_ptr<json::Document> event; ///< Event

public:
    /**
     * @brief Construct a new Event Handler from event
     *
     * @param event
     */
    EventHandler(std::shared_ptr<json::Document> event)
        : event {event}
        , m_isDecoded {false}
    {
        // TODO Throw exception if shared_ptr is empty
    }

    /**
     * @brief Get the Event
     *
     * @return std::shared_ptr<json::Document>
     */
    std::shared_ptr<json::Document> getEvent(void) const { return event; }

    /**
     * @brief Set a Key-Value in the Event
     * @param key
     * @param value
     *
     * @return bool
     */
    bool setEventValue(const std::string& key, rapidjson::Value& value)
    {
        return event->set(key, value);
    }

    /**
     * @brief Get an Event Value given its Key
     *
     * @param key Used to locate the value on the event
     * @return const rapidjson::Value&
     */
    const rapidjson::Value& getEventValue(const std::string& key)
    {
        return getEvent()->get(key);
    }

    /**
     * @brief Get the Event Doc Allocator object reference
     *
     * @return rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>&
     */
    rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>& getEventDocAllocator(void)
    {
        return getEvent()->m_doc.GetAllocator();
    }

    /**
     * @brief Checks if the event was decoded
     *
     * @return return true if it reached the end of the decoding stage
     */
    bool isDecoded(void) const { return m_isDecoded; }

    /**
     * @brief Changes event status to decoded
     */
    void setDecoded(void) { m_isDecoded = true; }
};

} // namespace base
#endif
