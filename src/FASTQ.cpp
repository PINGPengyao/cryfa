/**
 * @file      FASTQ.cpp
 * @brief     Compression/Decompression of FASTQ
 * @author    Morteza Hosseini  (seyedmorteza@ua.pt)
 * @author    Diogo Pratas      (pratas@ua.pt)
 * @author    Armando J. Pinho  (ap@ua.pt)
 * @copyright The GNU General Public License v3.0
 */

#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>       // time
#include <iomanip>      // setw, setprecision
#include <cstring>
#include "FASTQ.h"

//#include <functional>
//#include <algorithm>

using std::chrono::high_resolution_clock;
using std::thread;
using std::cout;
using std::cerr;
using std::ifstream;
using std::ofstream;
using std::to_string;
using std::setprecision;
using std::memset;

std::mutex mutxFQ;    /**< @brief Mutex */


/**
 * @brief Compress FASTQ
 */
void FASTQ::compressFQ ()
{
    // Start timer for compression
    high_resolution_clock::time_point startTime = high_resolution_clock::now();
    
    string   line;
    thread   arrThread[n_threads];
    byte     t;                   // For threads
    string   headers, qscores;
    packfq_s pkStruct;            // Collection of inputs to pass to pack...
    
    if (verbose)    cerr << "Calculating number of different characters...\n";
    
    // Gather different chars and max length in all headers and quality scores
    gatherHdrQs(headers, qscores);
    
    const size_t headersLen = headers.length();
    const size_t qscoresLen = qscores.length();
    
    // Show number of different chars in headers and qs -- Ignore '@'=64 in hdr
    if (verbose)
        cerr << "In headers, they are " << headersLen << ".\n"
             << "In quality scores, they are " << qscoresLen << ".\n";
    
    // Function pointers
    using packHdrPointer = void (EnDecrypto::*)
            (string&, const string&, const htbl_t&);
    packHdrPointer packHdr;
    using packQSPointer  = void (EnDecrypto::*)
            (string&, const string&, const htbl_t&);
    packQSPointer  packQS;
    
    // Header
    if (headersLen > MAX_C5)          // If len > 39 filter the last 39 ones
    {
        Hdrs = headers.substr(headersLen - MAX_C5);
//        Hdrs_g = Hdrs;
        // ASCII char after the last char in Hdrs -- Always <= (char) 127
        HdrsX = Hdrs;    HdrsX += (char) (Hdrs.back() + 1);
        buildHashTable(HdrMap, HdrsX, KEYLEN_C5);
        packHdr=&EnDecrypto::packLargeHdr_3to2;
    }
    else
    {
        Hdrs = headers;
//        Hdrs_g = Hdrs;
        
        if (headersLen > MAX_C4)                            // 16 <= cat 5 <= 39
        {
            buildHashTable(HdrMap, Hdrs, KEYLEN_C5);
            packHdr = &EnDecrypto::pack_3to2;
        }
        else if (headersLen > MAX_C3)                       // 7 <= cat 4 <= 15
        {
            buildHashTable(HdrMap, Hdrs, KEYLEN_C4);
            packHdr = &EnDecrypto::pack_2to1;
        }
        else if (headersLen==MAX_C3 || headersLen==MID_C3   // 4 <= cat 3 <= 6
                 || headersLen==MIN_C3)
        {
            buildHashTable(HdrMap, Hdrs, KEYLEN_C3);
            packHdr = &EnDecrypto::pack_3to1;
        }
        else if (headersLen == C2)                          // cat 2 = 3
        {
            buildHashTable(HdrMap, Hdrs, KEYLEN_C2);
            packHdr = &EnDecrypto::pack_5to1;
        }
        else if (headersLen == C1)                          // cat 1 = 2
        {
            buildHashTable(HdrMap, Hdrs, KEYLEN_C1);
            packHdr = &EnDecrypto::pack_7to1;
        }
        else                                                // headersLen = 1
        {
            buildHashTable(HdrMap, Hdrs, 1);
            packHdr = &EnDecrypto::pack_1to1;
        }
    }
    
    // Quality score
    if (qscoresLen > MAX_C5)              // If len > 39 filter the last 39 ones
    {
        QSs = qscores.substr(qscoresLen - MAX_C5);
//        QSs_g = QSs;
        // ASCII char after last char in QUALITY_SCORES
        QSsX = QSs;     QSsX += (char) (QSs.back() + 1);
        buildHashTable(QsMap, QSsX, KEYLEN_C5);
        packQS = &EnDecrypto::packLargeQs_3to2;
    }
    else
    {
        QSs = qscores;
//        QSs_g = QSs;
        
        if (qscoresLen > MAX_C4)                            // 16 <= cat 5 <= 39
        {
            buildHashTable(QsMap, QSs, KEYLEN_C5);
            packQS = &EnDecrypto::pack_3to2;
        }
        else if (qscoresLen > MAX_C3)                       // 7 <= cat 4 <= 15
        {
            buildHashTable(QsMap, QSs, KEYLEN_C4);
            packQS = &EnDecrypto::pack_2to1;
        }
        else if (qscoresLen==MAX_C3 || qscoresLen==MID_C3   // 4 <= cat 3 <= 6
                 || qscoresLen==MIN_C3)
        {
            buildHashTable(QsMap, QSs, KEYLEN_C3);
            packQS = &EnDecrypto::pack_3to1;
        }
        else if (qscoresLen == C2)                          // cat 2 = 3
        {
            buildHashTable(QsMap, QSs, KEYLEN_C2);
            packQS = &EnDecrypto::pack_5to1;
        }
        else if (qscoresLen == C1)                          // cat 1 = 2
        {
            buildHashTable(QsMap, QSs, KEYLEN_C1);
            packQS = &EnDecrypto::pack_7to1;
        }
        else                                                // qscoresLen = 1
        {
            buildHashTable(QsMap, QSs, 1);
            packQS = &EnDecrypto::pack_1to1;
        }
    }
    
    pkStruct.packHdrFPtr = packHdr;
    pkStruct.packQSFPtr  = packQS;
    
    // Distribute file among threads, for reading and packing
    for (t = 0; t != n_threads; ++t)
        arrThread[t] = thread(&FASTQ::packFQ, this, pkStruct, t);
    for (t = 0; t != n_threads; ++t)
        if (arrThread[t].joinable())    arrThread[t].join();
    
    if (verbose)    cerr << "Shuffling done!\n";
    
    // Join partially packed files
    ifstream pkFile[n_threads];
    string context;
    
    // Watermark for encrypted file
    cout << "#cryfa v" + to_string(VERSION_CRYFA) + "."
            + to_string(RELEASE_CRYFA) + "\n";
    
    // Open packed file
    ofstream pckdFile(PCKD_FILENAME);
    pckdFile << (!disable_shuffle ? (char) 128 : (char) 129); //Shuffling on/off
    pckdFile << headers;                            // Send headers to decryptor
    pckdFile << (char) 254;                         // To detect headers in dec.
    pckdFile << qscores;                            // Send qscores to decryptor
    pckdFile << (hasFQjustPlus() ? (char) 253 : '\n');        // If just '+'
    
    // Open input files
    for (t = 0; t != n_threads; ++t)  pkFile[t].open(PK_FILENAME+to_string(t));
    
    bool prevLineNotThrID;                 // If previous line was "THR=" or not
    while (!pkFile[0].eof())
    {
        for (t = 0; t != n_threads; ++t)
        {
            prevLineNotThrID = false;
            
            while (getline(pkFile[t], line).good() &&
                   line != THR_ID_HDR+to_string(t))
            {
                if (prevLineNotThrID)   pckdFile << '\n';
                pckdFile << line;
                
                prevLineNotThrID = true;
            }
        }
    }
    pckdFile << (char) 252;
    
    // Close/delete input/output files
    pckdFile.close();
    string pkFileName;
    for (t = 0; t != n_threads; ++t)
    {
        pkFile[t].close();
        pkFileName=PK_FILENAME;    pkFileName+=to_string(t);
        std::remove(pkFileName.c_str());
    }
    
    // Stop timer for compression
    high_resolution_clock::time_point finishTime = high_resolution_clock::now();
    // Compression duration in seconds
    std::chrono::duration<double> elapsed = finishTime - startTime;
    
    cerr << (verbose ? "Compaction done," : "Done,") << " in "
         << std::fixed << setprecision(4) << elapsed.count() << " seconds.\n";
    
    // Cout encrypted content
    encrypt();
    
    
    /*
    // get size of file
    infile.seekg(0, infile.end);
    long size = 1000000;//infile.tellg();
    infile.seekg(0);

    // allocate memory for file content
    char *buffer = new char[size];

    // read content of infile
    infile.read(buffer, size);

    // write to outfile
    outfile.write(buffer, size);

    // release dynamically-allocated memory
    delete[] buffer;

    outfile.close();
    infile.close();
    */
}

/**
 * @brief Pack FASTQ -- '@' at the beginning of headers is not packed
 * @param pkStruct  Pack structure
 * @param threadID  Thread ID
 */
void FASTQ::packFQ (const packfq_s& pkStruct, byte threadID)
{
    // Function pointers
    using packHdrFPtr   = void (EnDecrypto::*)
            (string&, const string&, const htbl_t&);
    packHdrFPtr packHdr = pkStruct.packHdrFPtr;
    using packQSPtr     = void (EnDecrypto::*)
            (string&, const string&, const htbl_t&);
    packQSPtr   packQS  = pkStruct.packQSFPtr;
    
    ifstream in(inFileName);
    string   context;       // Output string
    string   line;
    ofstream pkfile(PK_FILENAME+to_string(threadID), std::ios_base::app);
    
    // Lines ignored at the beginning
    for (u64 l = (u64) threadID*BlockLine; l--;)    IGNORE_THIS_LINE(in);
    
    while (in.peek() != EOF)
    {
        context.clear();
        
        for (u64 l = 0; l != BlockLine; l += 4)  // Process 4 lines by 4 lines
        {
            if (getline(in, line).good())          // Header -- Ignore '@'
            {
                (this->*packHdr) (context, line.substr(1), HdrMap);
                context+=(char) 254;
            }
            if (getline(in, line).good())          // Sequence
            {
                packSeq_3to1(context, line);
                context+=(char) 254;
            }
            IGNORE_THIS_LINE(in);                  // +. ignore
            if (getline(in, line).good())          // Quality score
            {
                (this->*packQS) (context, line, QsMap);
                context+=(char) 254;
            }
        }
        
        // shuffle
        if (!disable_shuffle)
        {
            mutxFQ.lock();//------------------------------------------------------
            if (verbose && shuffInProgress)    cerr << "Shuffling...\n";
            
            shuffInProgress = false;
            mutxFQ.unlock();//----------------------------------------------------
            
            shufflePkd(context);
        }
        
        // For unshuffling: insert the size of packed context in the beginning
        string contextSize;
        contextSize += (char) 253;
        contextSize += to_string(context.size());
        contextSize += (char) 254;
        context.insert(0, contextSize);
        
        // Write header containing threadID for each
        pkfile << THR_ID_HDR << to_string(threadID) << '\n';
        pkfile << context << '\n';
        
        // Ignore to go to the next related chunk
        for (u64 l = (u64) (n_threads-1)*BlockLine; l--;)  IGNORE_THIS_LINE(in);
    }
    
    pkfile.close();
    in.close();
}

/**
 * @brief      Gather chars of all headers & quality scores
 *             in FASTQ, excluding '@' in headers
 * @param[out] headers  Chars of all headers
 * @param[out] qscores  Chars of all quality scores
 */
void FASTQ::gatherHdrQs (string& headers, string& qscores)
{
    u32  maxHLen=0,   maxQLen=0;       // Max length of headers & quality scores
    bool hChars[127], qChars[127];
    std::memset(hChars+32, false, 95);
    std::memset(qChars+32, false, 95);
    
    ifstream in(inFileName);
    string line;
    while (!in.eof())
    {
        if (getline(in, line).good())
        {
            for (const char &c : line)    hChars[c] = true;
            if (line.size() > maxHLen)    maxHLen = (u32) line.size();
        }
        
        IGNORE_THIS_LINE(in);    // Ignore sequence
        IGNORE_THIS_LINE(in);    // Ignore +
        
        if (getline(in, line).good())
        {
            for (const char &c : line)    qChars[c] = true;
            if (line.size() > maxQLen)    maxQLen = (u32) line.size();
        }
    }
    in.close();
    
    // Number of lines read from input file while compression
    BlockLine = (u32) (4 * (BLOCK_SIZE / (maxHLen + 2*maxQLen)));
    if (!BlockLine)   BlockLine = 4;
    
    // Gather the characters -- ignore '@'=64 for headers
    for (byte i = 32; i != 64;  ++i)    if (*(hChars+i))  headers += i;
    for (byte i = 65; i != 127; ++i)    if (*(hChars+i))  headers += i;
    for (byte i = 32; i != 127; ++i)    if (*(qChars+i))  qscores += i;
    
    
    /* IDEA -- Slower
    u32 hL=0, qL=0;
    u64 hH=0, qH=0;
    string headers, qscores;
    ifstream in(inFileName);
    string line;
    while (!in.eof())
    {
        if (getline(in, line).good())
            for (const char &c : line)
                (c & 0xC0) ? (hH |= 1ULL<<(c-64)) : (hL |= 1U<<(c-32));
        IGNORE_THIS_LINE(in);    // ignore sequence
        IGNORE_THIS_LINE(in);    // ignore +
        if (getline(in, line).good())
            for (const char &c : line)
                (c & 0xC0) ? (qH |= 1ULL<<(c-64)) : (qL |= 1U<<(c-32));
    }
    in.close();

    // gather the characters -- ignore '@'=64 for headers
    for (byte i = 0; i != 32; ++i)    if (hL>>i & 1)  headers += i+32;
    for (byte i = 1; i != 62; ++i)    if (hH>>i & 1)  headers += i+64;
    for (byte i = 0; i != 32; ++i)    if (qL>>i & 1)  qscores += i+32;
    for (byte i = 1; i != 62; ++i)    if (qH>>i & 1)  qscores += i+64;
    */
}

/**
 * @brief Decompress FASTQ
 */
void FASTQ::decompressFQ ()
{
    // Start timer for decompression
    high_resolution_clock::time_point startTime = high_resolution_clock::now();
    
    char       c;                   // Chars in file
    string     headers, qscores;
    unpackfq_s upkStruct;           // Collection of inputs to pass to unpack...
    string     chunkSizeStr;        // Chunk size (string) -- For unshuffling
    thread     arrThread[n_threads];// Array of threads
    byte       t;                   // For threads
    u64        offset;              // To traverse decompressed file
    
    ifstream in(DEC_FILENAME);
    in.get(c);    shuffled = (c==(char) 128); // Check if file had been shuffled
    while (in.get(c) && c != (char) 254)                 headers += c;
    while (in.get(c) && c != '\n' && c != (char) 253)    qscores += c;
    if (c == '\n')    justPlus = false;                 // If 3rd line is just +
    
    const size_t headersLen = headers.length();
    const size_t qscoresLen = qscores.length();
    u16 keyLen_hdr = 0,  keyLen_qs = 0;
    
    // Show number of different chars in headers and qs -- ignore '@'=64
    if (verbose)
        cerr << headersLen << " different characters are in headers.\n"
             << qscoresLen << " different characters are in quality scores.\n";
    
    using unpackHdrPtr =
    void (EnDecrypto::*) (string&, string::iterator&, const vector<string>&);
    unpackHdrPtr unpackHdr;                                  // Function pointer
    using unpackQSPtr =
    void (EnDecrypto::*) (string&, string::iterator&, const vector<string>&);
    unpackQSPtr  unpackQS;                                   // Function pointer
    
    // Header
    if      (headersLen > MAX_C5)           keyLen_hdr = KEYLEN_C5;
    else if (headersLen > MAX_C4)                                       // Cat 5
    {
        unpackHdr  = &EnDecrypto::unpack_read2B;
        keyLen_hdr = KEYLEN_C5;
    }
    else
    {
        unpackHdr  = &EnDecrypto::unpack_read1B;
        
        if      (headersLen > MAX_C3)       keyLen_hdr = KEYLEN_C4;     // Cat 4
        else if (headersLen==MAX_C3 || headersLen==MID_C3 || headersLen==MIN_C3)
            keyLen_hdr = KEYLEN_C3;     // Cat 3
        else if (headersLen == C2)          keyLen_hdr = KEYLEN_C2;     // Cat 2
        else if (headersLen == C1)          keyLen_hdr = KEYLEN_C1;     // Cat 1
        else                                keyLen_hdr = 1;             // = 1
    }
    
    // Quality score
    if      (qscoresLen > MAX_C5)           keyLen_qs = KEYLEN_C5;
    else if (qscoresLen > MAX_C4)                                   // Cat 5
    {
        unpackQS  = &EnDecrypto::unpack_read2B;
        keyLen_qs = KEYLEN_C5;
    }
    else
    {
        unpackQS  = &EnDecrypto::unpack_read1B;
        
        if      (qscoresLen > MAX_C3)       keyLen_qs = KEYLEN_C4;      // Cat 4
        else if (qscoresLen==MAX_C3 || qscoresLen==MID_C3 || qscoresLen==MIN_C3)
            keyLen_qs = KEYLEN_C3;      // Cat 3
        else if (qscoresLen == C2)          keyLen_qs = KEYLEN_C2;      // Cat 2
        else if (qscoresLen == C1)          keyLen_qs = KEYLEN_C1;      // Cat 1
        else                                keyLen_qs = 1;              // = 1
    }
    
    string plusMore;
    if (headersLen <= MAX_C5 && qscoresLen <= MAX_C5)
    {
        // Tables for unpacking
        buildUnpack(upkStruct.hdrUnpack, headers, keyLen_hdr);
        buildUnpack(upkStruct.qsUnpack,  qscores, keyLen_qs);
        
        // Distribute file among threads, for reading and unpacking
        for (t = 0; t != n_threads; ++t)
        {
            in.get(c);
            if (c == (char) 253)
            {
                chunkSizeStr.clear();   // Chunk size
                while (in.get(c) && c != (char) 254)    chunkSizeStr += c;
                offset = stoull(chunkSizeStr);
                
                upkStruct.begPos        = in.tellg();
                upkStruct.chunkSize     = offset;
                upkStruct.unpackHdrFPtr = unpackHdr;
                upkStruct.unpackQSFPtr  = unpackQS;
                
                arrThread[t] = thread(&FASTQ::unpackHSQS, this, upkStruct, t);
                
                // Jump to the beginning of the next chunk
                in.seekg((std::streamoff) offset, std::ios_base::cur);
            }
            // End of file
            if (in.peek() == 252)    break;
        }
        // Join threads
        for (t = 0; t != n_threads; ++t)
            if (arrThread[t].joinable())    arrThread[t].join();
        
        if (verbose)    cerr << "Unshuffling done!\n";
    }
    else if (headersLen <= MAX_C5 && qscoresLen > MAX_C5)
    {
        const string decQscores = qscores.substr(qscoresLen - MAX_C5);
        // ASCII char after the last char in decQscores string
        string decQscoresX  = decQscores;
        decQscoresX += (upkStruct.XChar_qs = (char) (decQscores.back() + 1));
        
        // Tables for unpacking
        buildUnpack(upkStruct.hdrUnpack, headers,     keyLen_hdr);
        buildUnpack(upkStruct.qsUnpack,  decQscoresX, keyLen_qs);
        
        // Distribute file among threads, for reading and unpacking
        for (t = 0; t != n_threads; ++t)
        {
            in.get(c);
            if (c == (char) 253)
            {
                chunkSizeStr.clear();   // Chunk size
                while (in.get(c) && c != (char) 254)    chunkSizeStr += c;
                offset = stoull(chunkSizeStr);
                
                upkStruct.begPos        = in.tellg();
                upkStruct.chunkSize     = offset;
                upkStruct.unpackHdrFPtr = unpackHdr;
                
                arrThread[t] = thread(&FASTQ::unpackHSQL, this, upkStruct, t);
                
                // Jump to the beginning of the next chunk
                in.seekg((std::streamoff) offset, std::ios_base::cur);
            }
            // End of file
            if (in.peek() == 252)    break;
        }
        // Join threads
        for (t = 0; t != n_threads; ++t)
            if (arrThread[t].joinable())    arrThread[t].join();
        
        if (verbose)    cerr << "Unshuffling done!\n";
    }
    else if (headersLen > MAX_C5 && qscoresLen > MAX_C5)
    {
        const string decHeaders = headers.substr(headersLen - MAX_C5);
        const string decQscores = qscores.substr(qscoresLen - MAX_C5);
        // ASCII char after the last char in headers & quality_scores string
        string decHeadersX = decHeaders;
        decHeadersX += (upkStruct.XChar_hdr = (char) (decHeaders.back() + 1));
        string decQscoresX = decQscores;
        decQscoresX += (upkStruct.XChar_qs  = (char) (decQscores.back() + 1));
        
        // Tables for unpacking
        buildUnpack(upkStruct.hdrUnpack, decHeadersX, keyLen_hdr);
        buildUnpack(upkStruct.qsUnpack,  decQscoresX, keyLen_qs);
        
        // Distribute file among threads, for reading and unpacking
        for (t = 0; t != n_threads; ++t)
        {
            in.get(c);
            if (c == (char) 253)
            {
                chunkSizeStr.clear();   // Chunk size
                while (in.get(c) && c != (char) 254)    chunkSizeStr += c;
                offset = stoull(chunkSizeStr);
                
                upkStruct.begPos    = in.tellg();
                upkStruct.chunkSize = offset;
                
                arrThread[t] = thread(&FASTQ::unpackHLQL, this, upkStruct, t);
                
                // Jump to the beginning of the next chunk
                in.seekg((std::streamoff) offset, std::ios_base::cur);
            }
            // End of file
            if (in.peek() == 252)    break;
        }
        // Join threads
        for (t = 0; t != n_threads; ++t)
            if (arrThread[t].joinable())    arrThread[t].join();
        
        if (verbose)    cerr << "Unshuffling done!\n";
    }
    else if (headersLen > MAX_C5 && qscoresLen <= MAX_C5)
    {
        const string decHeaders = headers.substr(headersLen - MAX_C5);
        // ASCII char after the last char in headers string
        string decHeadersX = decHeaders;
        decHeadersX += (upkStruct.XChar_hdr = (char) (decHeaders.back() + 1));
        
        // Tables for unpacking
        buildUnpack(upkStruct.hdrUnpack, decHeadersX, keyLen_hdr);
        buildUnpack(upkStruct.qsUnpack,  qscores,     keyLen_qs);
        
        // Distribute file among threads, for reading and unpacking
        for (t = 0; t != n_threads; ++t)
        {
            in.get(c);
            if (c == (char) 253)
            {
                chunkSizeStr.clear();   // Chunk size
                while (in.get(c) && c != (char) 254)    chunkSizeStr += c;
                offset = stoull(chunkSizeStr);
                
                upkStruct.begPos       = in.tellg();
                upkStruct.chunkSize    = offset;
                upkStruct.unpackQSFPtr = unpackQS;
                
                arrThread[t] = thread(&FASTQ::unpackHLQS, this, upkStruct, t);
                
                // Jump to the beginning of the next chunk
                in.seekg((std::streamoff) offset, std::ios_base::cur);
            }
            // End of file
            if (in.peek() == 252)    break;
        }
        // Join threads
        for (t = 0; t != n_threads; ++t)
            if (arrThread[t].joinable())    arrThread[t].join();
        
        if (verbose)    cerr << "Unshuffling done!\n";
    }
    
    // Close/delete decrypted file
    in.close();
    const string decFileName = DEC_FILENAME;
    std::remove(decFileName.c_str());
    
    // Join unpacked files
    ifstream upkdFile[n_threads];
    string line;
    for (t = n_threads; t--;)   upkdFile[t].open(UPK_FILENAME+to_string(t));
    
    bool prevLineNotThrID;            // If previous line was "THRD=" or not
    while (!upkdFile[0].eof())
    {
        for (t = 0; t != n_threads; ++t)
        {
            prevLineNotThrID = false;
            
            while (getline(upkdFile[t], line).good() &&
                   line != THR_ID_HDR+to_string(t))
            {
                if (prevLineNotThrID)
                    cout << '\n';
                cout << line;
                
                prevLineNotThrID = true;
            }
            
            if (prevLineNotThrID)    cout << '\n';
        }
    }
    
    // Stop timer for decompression
    high_resolution_clock::time_point finishTime = high_resolution_clock::now();
    // Decompression duration in seconds
    std::chrono::duration<double> elapsed = finishTime - startTime;
    
    cerr << (verbose ? "Decompression done," : "Done,") << " in "
         << std::fixed << setprecision(4) << elapsed.count() << " seconds.\n";
    
    // Close/delete input/output files
    string upkdFileName;
    for (t = n_threads; t--;)
    {
        upkdFile[t].close();
        upkdFileName=UPK_FILENAME;    upkdFileName+=to_string(t);
        std::remove(upkdFileName.c_str());
    }
}

/**
 * @brief Unpack FQ: small header, small quality score
 *        -- '@' at the beginning of headers not packed
 * @param upkStruct  Unpack structure
 * @param threadID   Thread ID
 */
void FASTQ::unpackHSQS (const unpackfq_s &upkStruct, byte threadID)
{
    // Function pointers
    using unpackHdrFPtr =
    void (EnDecrypto::*) (string&, string::iterator&, const vector<string>&);
    unpackHdrFPtr    unpackHdr = upkStruct.unpackHdrFPtr;
    using unpackQSFPtr  =
    void (EnDecrypto::*) (string&, string::iterator&, const vector<string>&);
    unpackQSFPtr     unpackQS  = upkStruct.unpackQSFPtr;
    pos_t            begPos    = upkStruct.begPos;
    u64              chunkSize = upkStruct.chunkSize;
    ifstream         in(DEC_FILENAME);
    string           decText, plusMore, chunkSizeStr;
    string::iterator i;
    char             c;
    pos_t            endPos;
    ofstream upkfile(UPK_FILENAME+to_string(threadID), std::ios_base::app);
    string upkHdrOut, upkSeqOut, upkQsOut;
    
    while (in.peek() != EOF)
    {
        in.seekg(begPos);      // Read the file from this position
        // Take a chunk of decrypted file
        decText.clear();
        for (u64 u = chunkSize; u--;) { in.get(c);    decText += c; }
        i = decText.begin();
        endPos = in.tellg();   // Set the end position
        
        // Unshuffle
        if (shuffled)
        {
            mutxFQ.lock();//------------------------------------------------------
            if (verbose && shuffInProgress)    cerr << "Unshuffling...\n";
            
            shuffInProgress = false;
            mutxFQ.unlock();//----------------------------------------------------
            
            unshufflePkd(i, chunkSize);
        }
        
        upkfile << THR_ID_HDR + to_string(threadID) << '\n';
        do {
            upkfile << '@';
            
            (this->*unpackHdr) (upkHdrOut, i, upkStruct.hdrUnpack);
            upkfile << (plusMore = upkHdrOut) << '\n';              ++i;  // Hdr
            
            unpackSeqFQ_3to1(upkSeqOut, i);
            upkfile << upkSeqOut << '\n';                                 // Seq
            
            upkfile << (justPlus ? "+" : "+" + plusMore) << '\n';   ++i;  // +
            
            (this->*unpackQS) (upkQsOut, i, upkStruct.qsUnpack);
            upkfile << upkQsOut << '\n';                                  // Qs
        } while (++i != decText.end());        // If trouble: change "!=" to "<"
        
        // Update the chunk size and positions (beg & end)
        for (byte t = n_threads; t--;)
        {
            in.seekg(endPos);
            in.get(c);
            if (c == (char) 253)
            {
                chunkSizeStr.clear();
                while (in.get(c) && c != (char) 254)    chunkSizeStr += c;
                
                chunkSize = stoull(chunkSizeStr);
                begPos    = in.tellg();
                endPos    = begPos + (pos_t) chunkSize;
            }
        }
    }
    
    upkfile.close();
    in.close();
}

/**
 * @brief Unpack FQ: small header, large quality score
 *        -- '@' at the beginning of headers not packed
 * @param upkStruct  Unpack structure
 * @param threadID   Thread ID
 */
void FASTQ::unpackHSQL (const unpackfq_s &upkStruct, byte threadID)
{
    using unpackHdrFPtr =
    void (EnDecrypto::*) (string&, string::iterator&, const vector<string>&);
    unpackHdrFPtr    unpackHdr = upkStruct.unpackHdrFPtr;    // Function pointer
    pos_t            begPos    = upkStruct.begPos;
    u64              chunkSize = upkStruct.chunkSize;
    ifstream         in(DEC_FILENAME);
    string           decText, plusMore, chunkSizeStr;
    string::iterator i;
    char             c;
    pos_t            endPos;
    ofstream upkfile(UPK_FILENAME + to_string(threadID), std::ios_base::app);
    string upkHdrOut, upkSeqOut, upkQsOut;
    
    while (in.peek() != EOF)
    {
        in.seekg(begPos);       // Read file from this position
        // Take a chunk of decrypted file
        decText.clear();
        for (u64 u = chunkSize; u--;) { in.get(c);    decText += c; }
        i = decText.begin();
        endPos = in.tellg();    // Set the end position
        
        // Unshuffle
        if (shuffled)
        {
            mutxFQ.lock();//------------------------------------------------------
            if (verbose && shuffInProgress)    cerr << "Unshuffling...\n";
            
            shuffInProgress = false;
            mutxFQ.unlock();//----------------------------------------------------
            
            unshufflePkd(i, chunkSize);
        }
        
        upkfile << THR_ID_HDR + to_string(threadID) << '\n';
        do {
            upkfile << '@';
            
            (this->*unpackHdr) (upkHdrOut, i, upkStruct.hdrUnpack);
            upkfile << (plusMore = upkHdrOut) << '\n';               ++i; // Hdr
            
            unpackSeqFQ_3to1(upkSeqOut, i);
            upkfile << upkSeqOut << '\n';                                 // Seq
            
            upkfile << (justPlus ? "+" : "+" + plusMore) << '\n';    ++i; // +
            
            unpackLarge_read2B(upkQsOut, i,
                               upkStruct.XChar_qs, upkStruct.qsUnpack);
            upkfile << upkQsOut << '\n';                                  // Qs
        } while (++i != decText.end());        // If trouble: change "!=" to "<"
        
        // Update the chunk size and positions (beg & end)
        for (byte t = n_threads; t--;)
        {
            in.seekg(endPos);
            in.get(c);
            if (c == (char) 253)
            {
                chunkSizeStr.clear();
                while (in.get(c) && c != (char) 254)    chunkSizeStr += c;
                
                chunkSize = stoull(chunkSizeStr);
                begPos    = in.tellg();
                endPos    = begPos + (pos_t) chunkSize;
            }
        }
    }
    
    upkfile.close();
    in.close();
}

/**
 * @brief Unpack FQ: large header, small quality score
 *        -- '@' at the beginning of headers not packed
 * @param upkStruct  Unpack structure
 * @param threadID   Thread ID
 */
void FASTQ::unpackHLQS (const unpackfq_s &upkStruct, byte threadID)
{
    using unpackQSFPtr =
    void (EnDecrypto::*) (string&, string::iterator&, const vector<string>&);
    unpackQSFPtr     unpackQS = upkStruct.unpackQSFPtr;      // Function pointer
    pos_t            begPos    = upkStruct.begPos;
    u64              chunkSize = upkStruct.chunkSize;
    ifstream         in(DEC_FILENAME);
    string           decText, plusMore, chunkSizeStr;
    string::iterator i;
    char             c;
    pos_t            endPos;
    ofstream upkfile(UPK_FILENAME + to_string(threadID), std::ios_base::app);
    string upkHdrOut, upkSeqOut, upkQsOut;
    
    while (in.peek() != EOF)
    {
        in.seekg(begPos);       // Read file from this position
        // Take a chunk of decrypted file
        decText.clear();
        for (u64 u = chunkSize; u--;) { in.get(c);    decText += c; }
        i = decText.begin();
        endPos = in.tellg();    // Set the end position
        
        // Unshuffle
        if (shuffled)
        {
            mutxFQ.lock();//------------------------------------------------------
            if (verbose && shuffInProgress)    cerr << "Unshuffling...\n";
            
            shuffInProgress = false;
            mutxFQ.unlock();//----------------------------------------------------
            
            unshufflePkd(i, chunkSize);
        }
        
        upkfile << THR_ID_HDR + to_string(threadID) << '\n';
        do {
            upkfile << '@';
            
            unpackLarge_read2B(upkHdrOut, i,
                               upkStruct.XChar_hdr, upkStruct.hdrUnpack);
            upkfile << (plusMore = upkHdrOut) << '\n';              ++i;  // Hdr
            
            unpackSeqFQ_3to1(upkSeqOut, i);
            upkfile << upkSeqOut << '\n';                                 // Seq
            
            upkfile << (justPlus ? "+" : "+" + plusMore) << '\n';   ++i;  // +
            
            (this->*unpackQS) (upkQsOut, i, upkStruct.qsUnpack);
            upkfile << upkQsOut << '\n';                                  // Qs
        } while (++i != decText.end());        // If trouble: change "!=" to "<"
        
        // Update the chunk size and positions (beg & end)
        for (byte t = n_threads; t--;)
        {
            in.seekg(endPos);
            in.get(c);
            if (c == (char) 253)
            {
                chunkSizeStr.clear();
                while (in.get(c) && c != (char) 254)    chunkSizeStr += c;
                
                chunkSize = stoull(chunkSizeStr);
                begPos    = in.tellg();
                endPos    = begPos + (pos_t) chunkSize;
            }
        }
    }
    
    upkfile.close();
    in.close();
}

/**
 * @brief Unpack FQ: large header, large quality score
 *        -- '@' at the beginning of headers not packed
 * @param upkStruct  Unpack structure
 * @param threadID   Thread ID
 */
void FASTQ::unpackHLQL (const unpackfq_s &upkStruct, byte threadID)
{
    pos_t            begPos    = upkStruct.begPos;
    u64              chunkSize = upkStruct.chunkSize;
    ifstream         in(DEC_FILENAME);
    string           decText, plusMore, chunkSizeStr;
    string::iterator i;
    char             c;
    pos_t            endPos;
    ofstream upkfile(UPK_FILENAME + to_string(threadID), std::ios_base::app);
    string upkHdrOut, upkSeqOut, upkQsOut;
    
    while (in.peek() != EOF)
    {
        in.seekg(begPos);       // Read file from this position
        // Take a chunk of decrypted file
        decText.clear();
        for (u64 u = chunkSize; u--;) { in.get(c);    decText += c; }
        i = decText.begin();
        endPos = in.tellg();    // Set the end position
        
        // Unshuffle
        if (shuffled)
        {
            mutxFQ.lock();//------------------------------------------------------
            if (verbose && shuffInProgress)    cerr << "Unshuffling...\n";
            
            shuffInProgress = false;
            mutxFQ.unlock();//----------------------------------------------------
            
            unshufflePkd(i, chunkSize);
        }
        
        upkfile << THR_ID_HDR + to_string(threadID) << '\n';
        do {
            upkfile << '@';
            
            unpackLarge_read2B(upkHdrOut, i,
                               upkStruct.XChar_hdr, upkStruct.hdrUnpack);
            upkfile << (plusMore = upkHdrOut) << '\n';              ++i;  // Hdr
            
            unpackSeqFQ_3to1(upkSeqOut, i);
            upkfile << upkSeqOut << '\n';                                 // Seq
            
            upkfile << (justPlus ? "+" : "+" + plusMore) << '\n';   ++i;  // +
            
            unpackLarge_read2B(upkQsOut, i,
                               upkStruct.XChar_qs, upkStruct.qsUnpack);
            upkfile << upkQsOut << '\n';                                  // Qs
        } while (++i != decText.end());        // If trouble: change "!=" to "<"
        
        // Update the chunk size and positions (beg & end)
        for (byte t = n_threads; t--;)
        {
            in.seekg(endPos);
            in.get(c);
            if (c == (char) 253)
            {
                chunkSizeStr.clear();
                while (in.get(c) && c != (char) 254)    chunkSizeStr += c;
                
                chunkSize = stoull(chunkSizeStr);
                begPos    = in.tellg();
                endPos    = begPos + (pos_t) chunkSize;
            }
        }
    }
    
    upkfile.close();
    in.close();
}

/**
 * @brief      Unpack 1 byte to 3 DNA bases -- FASTQ
 * @param[out] out  Unpacked DNA bases
 * @param[in]  i    Input string iterator
 */
void FASTQ::unpackSeqFQ_3to1 (string &out, string::iterator &i)
{
    string tpl;    tpl.reserve(3);
    out.clear();
    
    for (; *i != (char) 254; ++i)
    {
        // Seq len not multiple of 3
        if (*i == (char) 255) { out += penaltySym(*(++i));    continue; }
        
        tpl = DNA_UNPACK[(byte) *i];
        
        if (tpl[0]!='X' && tpl[1]!='X' && tpl[2]!='X')    out+=tpl;       // ...
        
        else if (tpl[0]=='X' && tpl[1]!='X' && tpl[2]!='X')               // X..
        { out+=penaltySym(*(++i));    out+=tpl[1];    out+=tpl[2];             }
        
        else if (tpl[0]!='X' && tpl[1]=='X' && tpl[2]!='X')               // .X.
        { out+=tpl[0];    out+=penaltySym(*(++i));    out+=tpl[2];             }
        
        else if (tpl[0]=='X' && tpl[1]=='X' && tpl[2]!='X')               // XX.
        { out+=penaltySym(*(++i));    out+=penaltySym(*(++i));    out+=tpl[2]; }
        
        else if (tpl[0]!='X' && tpl[1]!='X' && tpl[2]=='X')               // ..X
        { out+=tpl[0];    out+=tpl[1];    out+=penaltySym(*(++i));             }
        
        else if (tpl[0]=='X' && tpl[1]!='X' && tpl[2]=='X')               // X.X
        { out+=penaltySym(*(++i));    out+=tpl[1];    out+=penaltySym(*(++i)); }
        
        else if (tpl[0]!='X' && tpl[1]=='X' && tpl[2]=='X')               // .XX
        { out+=tpl[0];    out+=penaltySym(*(++i));    out+=penaltySym(*(++i)); }
        
        else { out+=penaltySym(*(++i));    out+=penaltySym(*(++i));       // XXX
            out+=penaltySym(*(++i));                                        }
    }
}