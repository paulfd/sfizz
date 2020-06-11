#include "Voice.h"

namespace sfz
{
/**
 * @brief RAII helper to build sister voice rings.
 * Closes the doubly-linked list on destruction.
 *
 */
class SisterVoiceRingBuilder {
public:
    ~SisterVoiceRingBuilder() {
        if (lastStartedVoice != nullptr) {
            ASSERT(firstStartedVoice);
            lastStartedVoice->setNextSisterVoice(firstStartedVoice);
            firstStartedVoice->setPreviousSisterVoice(lastStartedVoice);
        }
    }

    /**
     * @brief Add a voice to the sister ring
     *
     * @param voice
     */
    void addVoiceToRing(Voice* voice) {
        if (firstStartedVoice == nullptr)
            firstStartedVoice = voice;

        if (lastStartedVoice != nullptr) {
            voice->setPreviousSisterVoice(lastStartedVoice);
            lastStartedVoice->setNextSisterVoice(voice);
        }

        lastStartedVoice = voice;
    }
private:
    Voice* firstStartedVoice { nullptr };
    Voice* lastStartedVoice { nullptr };
};

}
