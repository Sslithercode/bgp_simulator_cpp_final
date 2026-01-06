#ifndef BGP_POLICY_H
#define BGP_POLICY_H

#include "announcement.h"
#include <unordered_map>
#include <vector>

// Abstract BGP Policy class
class BGPPolicy {
protected:
    // Local RIB: prefix -> best announcement
    // Using unordered_map for O(1) lookups
    std::unordered_map<Prefix, Announcement> local_rib;

    // Received queue: prefix -> list of received announcements
    // Cleared after processing
    std::unordered_map<Prefix, std::vector<Announcement>> received_queue;

public:
    virtual ~BGPPolicy() = default;

    // Receive an announcement (add to received queue)
    virtual void receiveAnnouncement(const Announcement& ann);

    // Process received queue and update local RIB
    // current_asn: ASN to prepend to paths when storing
    // Returns: true if any announcements changed
    virtual bool processReceivedQueue(ASN current_asn);

    // Get announcement from local RIB
    virtual const Announcement* getAnnouncement(const Prefix& prefix) const;

    // Get all announcements in local RIB
    virtual const std::unordered_map<Prefix, Announcement>& getLocalRIB() const {
        return local_rib;
    }

    // Clear received queue
    virtual void clearReceivedQueue();

    // Seed an announcement directly into local RIB (for origin ASes)
    virtual void seedAnnouncement(const Announcement& ann);

    // Get statistics
    size_t getLocalRIBSize() const { return local_rib.size(); }
    size_t getReceivedQueueSize() const { return received_queue.size(); }
};

// Standard BGP implementation
class BGP : public BGPPolicy {
public:
    // Inherited methods use default BGP behavior
    bool processReceivedQueue(ASN current_asn) override;
};

// ROV (Route Origin Validation) - extends BGP with ROV defense
class ROV : public BGP {
public:
    // Override to filter rov_invalid announcements
    void receiveAnnouncement(const Announcement& ann) override;

    // Statistics
    size_t getDroppedCount() const { return dropped_count; }

private:
    size_t dropped_count = 0;
};

#endif // BGP_POLICY_H
