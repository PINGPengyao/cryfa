/**
 * @file      Security.hpp
 * @brief     Security
 * @author    Morteza Hosseini  (seyedmorteza@ua.pt)
 * @author    Diogo Pratas      (pratas@ua.pt)
 * @author    Armando J. Pinho  (ap@ua.pt)
 * @copyright The GNU General Public License v3.0
 */

#ifndef CRYFA_SECURITY_H
#define CRYFA_SECURITY_H

#include "def.hpp"


/**
 * @brief Security
 */
class Security : public InArgs
{
public:
    void decrypt       ();
    
protected:
    bool shuffInProgress=true;/**< @brief Shuffle in progress @hideinitializer*/
    bool shuffled       =true;/**< @hideinitializer */

    void encrypt       ();
    void shuffle       (string&);
    void unshuffle     (string::iterator&, u64);
    
private:
    u64  seed_shared;         /**< @brief Shared seed */
//    const int TAG_SIZE = 12;  /**< @brief Tag size used in GCC mode auth enc */
    
    string extractPass ()                       const;
    void newSrand      (u32);
    int  newRand       ();
    std::minstd_rand0 &randomEngine ();
    void shuffSeedGen  ();
    void buildIV       (byte*, const string&);
    void buildKey      (byte*, const string&);

#ifdef DEBUG
    void printIV       (byte*)                  const;
    void printKey      (byte*)                  const;
#endif
};

#endif //CRYFA_SECURITY_H