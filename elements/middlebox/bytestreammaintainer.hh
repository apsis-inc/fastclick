#ifndef MIDDLEBOX_BYTESTREAMMAINTAINER_HH
#define MIDDLEBOX_BYTESTREAMMAINTAINER_HH

#include <click/config.h>
#include <click/glue.hh>
#include <clicknet/tcp.h>
#include "rbt.hh"
#include "memorypool.hh"

CLICK_DECLS

#define BS_TREE_POOL_SIZE 10
#define BS_POOL_SIZE 40
#define BS_PRUNE_THRESHOLD BS_POOL_SIZE / 2

class RBTMemoryPoolStreamManager : public RBTManager
{
public:
    RBTMemoryPoolStreamManager() : poolNodes(BS_POOL_SIZE),
        poolKeys(BS_POOL_SIZE),
        poolInfos(BS_POOL_SIZE),
        poolTrees(BS_TREE_POOL_SIZE)
    {

    }

    int compareKeys(const void* first, const void* second)
    {
        if(SEQ_GT(*(uint32_t*)first, *(uint32_t*)second))
            return 1;
        if(SEQ_LT(*(uint32_t*)first, *(uint32_t*)second))
            return -1;

        return 0;
    }

    void printKey(const void* first)
    {
        click_chatter("%u", *(uint32_t*)first);
    }

    void printInfo(void* first)
    {
        click_chatter("%d", *(int*)first);
    }

    rb_red_blk_node* allocateNode(void)
    {
        return poolNodes.getMemory();
    }

    rb_red_blk_tree* allocateTree(void)
    {
        return poolTrees.getMemory();
    }

    void freeNode(rb_red_blk_node* node)
    {
        poolNodes.releaseMemory(node);
    }

    void freeKey(void* key)
    {
        poolKeys.releaseMemory((uint32_t*)key);
    }

    void freeInfo(void* info)
    {
        poolInfos.releaseMemory((int*)info);
    }

    void freeTree(rb_red_blk_tree* tree)
    {
        poolTrees.releaseMemory(tree);
    }

    uint32_t* allocateKey(void)
    {
        return poolKeys.getMemory();
    }

    int* allocateInfo(void)
    {
        return poolInfos.getMemory();
    }

private:
    MemoryPool<rb_red_blk_tree> poolTrees;
    MemoryPool<rb_red_blk_node> poolNodes;
    MemoryPool<uint32_t> poolKeys;
    MemoryPool<int> poolInfos;
};

class ByteStreamMaintainer
{
    // ModificationList is the only one allowed to add nodes in the tress
    friend class ModificationList;

    public:
        ByteStreamMaintainer();
        ~ByteStreamMaintainer();
        uint32_t mapAck(uint32_t position);
        uint32_t mapSeq(uint32_t position);
        int lastOffsetInAckTree();
        void printTrees();
        void initialize(RBTMemoryPoolStreamManager *rbtManager, uint32_t flowStart);
        void prune(uint32_t position);

        void setLastAckSent(uint32_t ackNumber);
        uint32_t getLastAckSent();
        bool isLastAckSentSet();
        void setLastSeqSent(uint32_t seqNumber);
        uint32_t getLastSeqSent();
        bool isLastSeqSentSet();
        void setLastAckReceived(uint32_t ackNumber);
        uint32_t getLastAckReceived();
        bool isLastAckReceivedSet();
        uint8_t getDupAcks();
        void setDupAcks(uint8_t dupAcks);

        void setWindowSize(uint16_t windowSize);
        uint16_t getWindowSize();
        void setWindowScale(uint16_t windowScale);
        uint16_t getWindowScale();
        void setUseWindowScale(bool useWindowScale);
        bool getUseWindowScale();
        uint16_t getMSS();
        void setMSS(uint16_t mss);
        uint64_t getCongestionWindowSize();
        void setCongestionWindowSize(uint64_t congestionWindow);
        void setSsthresh(uint64_t ssthresh);
        uint64_t getSsthresh();
        void setIpSrc(uint32_t ipSrc);
        uint32_t getIpSrc();
        void setIpDst(uint32_t ipDst);
        uint32_t getIpDst();
        void setPortSrc(uint16_t portSrc);
        uint16_t getPortSrc();
        void setPortDst(uint16_t portDst);
        uint16_t getPortDst();

    private:
        void insertInAckTree(uint32_t position, int offset);
        void insertInSeqTree(uint32_t position, int offset);
        void insertInTree(rb_red_blk_tree* tree, uint32_t position, int offset);

        rb_red_blk_tree* treeAck;
        rb_red_blk_tree* treeSeq;
        RBTManager* rbtManager;
        uint32_t pruneCounter;
        bool initialized;
        bool lastAckSentSet;
        bool lastSeqSentSet;
        bool lastAckReceivedSet;

        uint32_t lastAckSent;     // /!\ mapped value (as sent)
        uint32_t lastSeqSent;     // /!\ mapped value (as sent)
        uint32_t lastAckReceived; // /!\ Unamapped value (as received)
        uint16_t windowSize;
        uint16_t windowScale;
        uint8_t dupAcks;
        bool useWindowScale;
        uint64_t congestionWindow;
        uint64_t ssthresh;
        uint16_t mss;
        uint32_t ipSrc;
        uint32_t ipDst;
        uint16_t portSrc;
        uint16_t portDst;

};

CLICK_ENDDECLS
#endif
