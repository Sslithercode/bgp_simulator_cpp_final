#include "bgp_policy.h"

void BGPPolicy::receiveAnnouncement(const Announcement& ann) {
    received_queue[ann.prefix].push_back(ann);
}

const Announcement* BGPPolicy::getAnnouncement(const Prefix& prefix) const {
    auto it = local_rib.find(prefix);
    return (it != local_rib.end()) ? &(it->second) : nullptr;
}

void BGPPolicy::clearReceivedQueue() {
    received_queue.clear();
}

void BGPPolicy::seedAnnouncement(const Announcement& ann) {
    local_rib[ann.prefix] = ann;
}

bool BGPPolicy::processReceivedQueue(ASN current_asn) {
    // Base implementation - should be overridden
    (void)current_asn; // Unused
    return false;
}

bool BGP::processReceivedQueue(ASN current_asn) {
    bool changed = false;

    for (auto& pair : received_queue) {
        const Prefix& prefix = pair.first;
        std::vector<Announcement>& candidates = pair.second;

        if (candidates.empty()) {
            continue;
        }

        // Find best announcement among candidates
        const Announcement* best = &candidates[0];
        for (size_t i = 1; i < candidates.size(); i++) {
            if (candidates[i].isBetterThan(*best)) {
                best = &candidates[i];
            }
        }

        // IMPORTANT: Prepend current ASN to the path when storing
        Announcement stored_ann = *best;
        stored_ann.as_path.insert(stored_ann.as_path.begin(), current_asn);

        // Check if we need to update local RIB
        auto rib_it = local_rib.find(prefix);

        if (rib_it == local_rib.end()) {
            // No existing announcement, add the best one (with prepended ASN)
            local_rib[prefix] = stored_ann;
            changed = true;
        } else {
            // Compare with existing announcement
            if (stored_ann.isBetterThan(rib_it->second)) {
                rib_it->second = stored_ann;
                changed = true;
            }
        }
    }

    return changed;
}

// ROV Implementation
void ROV::receiveAnnouncement(const Announcement& ann) {
    // Drop announcements with rov_invalid = true
    if (ann.rov_invalid) {
        dropped_count++;
        return; // Do not add to received queue
    }

    // Otherwise, use standard BGP behavior
    BGP::receiveAnnouncement(ann);
}
