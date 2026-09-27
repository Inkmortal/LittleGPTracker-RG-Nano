#ifndef _CHAIN_H_
#define _CHAIN_H_

#define CHAIN_COUNT 0xFF
#define NO_MORE_CHAIN 0x100

class Chain {
public:
	Chain() ;
	~Chain() ;
	unsigned short GetNext() ;
	// A new chain with the same phrases and transposes as src (the
	// phrases themselves are shared); NO_MORE_CHAIN when full
	unsigned short Clone(unsigned char src) ;
	bool IsUsed(unsigned char i) { return isUsed_[i] ; } ;
	void SetUsed(unsigned char c) ;
	void ClearAllocation() ;
	bool *UsedFlags() { return isUsed_ ; } ; // for undo snapshots

	unsigned char *data_ ;
	unsigned char *transpose_ ;

	
private:
	bool isUsed_[CHAIN_COUNT] ;

} ;

#endif
