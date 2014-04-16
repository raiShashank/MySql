#include "file.h"


file::file(const char *fname)
{
	success = false;
	if (exists(fname)){
		if (open(fname))
			success = true;
	}
	else {
		cerr << "File does not exist!!\n";
	}
}
file::file(const char *fname, blk_size bl_size, int offset = 0)
{
	success = false;
	if (exists(fname)){
		cerr << "File by this name already exists!!\n";
	}
	else {
		if (create(fname, bl_size, offset))
			success = true;
	}
}
file::~file()
{
	/* Since in dis program I can only set header's dirty bit, I cannot reset it. And used_block can only increase, hence whenevr used_block
		increases, header's dirty bit is set
	*/
	if (success){

		fseek(fp, _offset + 2 * sizeof(int), SEEK_SET);		//value of _offset, length of _offset, value of block_size
		fwrite(&header.used_block, sizeof(int), 1, fp);	
		if (header.dirty) {
			cout << "Dirty bitmap\n";
			fwrite(header.bitmap, sizeof(char), max_bytes, fp);
		}
		//implemented write_block using SEEK_CUR, amortized better performance
		for (iset set_iter = dirty_pages.begin(); set_iter != dirty_pages.end(); ++set_iter){	
			cout << "Dirty frame\n";
			write_block(*set_iter, page_tbl[*set_iter].buf);
		}
		delete[] header.bitmap;
		if (fp) fclose(fp);
	}
	fp = NULL;
}
/*
	Order of various things in file :
	1) length of offset (integer)
	2) offset no. bytes
	3) block_size (integer)
	4) no. of used_block (integer)
	5) bitmap (takes max_bytes no. of bytes)

	Header which consists of all the info which I need whenever file is opened : bitmap, block size, no. of used blocks in the file 
	I the structure , there is also a field bool dirty but it is not stored in the file
	the way I have implemented free_list, it first tries to fill holes b4 extending the file
*/
bool file::create(const char *fname, blk_size bl_size, int offset = 0) //bl_size nd offset are in number of bytes
{
	fp = fopen(fname, "wb+");
	if (!fp){
		cerr << "File could not be opened!!\n";
		return false;
	}
	max_block = MAX_SIZE / bl_size;		//maximum no. of blocks since each block corresponds to 1 bit max_block no. of bit
	max_bytes = max_block / CHAR_BIT;
	int size = max_bytes + 3 * sizeof(int) + offset;   			//size bytes are used up in the file for writing preliminary info

	cout << "Size of header: " << size << "\n";

	fseek(fp, size - 1, SEEK_SET);
	char c = '\0';
	fwrite(&c, sizeof(char), 1, fp);
	//till here file of size = size(header is created)

	unsigned char *bitmap = new unsigned char[max_bytes]; 	//one char corresponds to 8 bits
	fill (bitmap, bitmap + max_bytes, 0);

	
	header.dirty = false;
	header.bitmap = bitmap;
	header.bl_size = bl_size;
	header.used_block = 0;

	block_size = bl_size;
	_offset = offset;

	fseek(fp, 0, SEEK_SET);	
	fwrite(&_offset, sizeof (int), 1, fp);	
	cout << _offset << " " << ftell(fp) << "\n";
	fseek(fp, _offset, SEEK_CUR);
	cout<< ftell(fp) << "\n";
	fwrite(&header.bl_size, sizeof (int), 1, fp);	
	cout<< ftell(fp) << "\n";
	fwrite(&header.used_block, sizeof (int), 1, fp);	
	cout<< ftell(fp) << "\n";
	fwrite(header.bitmap, sizeof (char), max_bytes , fp);	
	
	cout << ftell(fp) << "\nOut of create()\n";

	free_list.push(header.used_block);
	return true;
	//fp now points to position after header
}
bool file::open(const char *fname)
{
	fp = fopen(fname, "rb+");
	if (!fp){
		cerr << "File could not be opened!!\n";
		return false;
	}
	
	cout << ftell(fp) << "\n";

	fread(&_offset, sizeof (int), 1, fp);
	cout << _offset << " " << ftell(fp) << "\n";
	fseek(fp, _offset, SEEK_CUR);
	cout << ftell(fp) << "\n";
	fread(&header.bl_size, sizeof (int), 1, fp);
	cout << "After reading block_size: " << ftell(fp) << "\n";
	fread(&header.used_block, sizeof (int), 1, fp);
	cout << "After reading used_block: " << ftell(fp) << "\n";

	max_block = MAX_SIZE / header.bl_size;		//maximum no. of blocks since each block corresponds to 1 bit max_block no. of bit
	max_bytes = max_block / CHAR_BIT;
	header.bitmap =  new unsigned char[max_bytes];

	fread(header.bitmap, sizeof(char), max_bytes, fp);		//initializing header
	cout << "After reading bitmap: " << ftell(fp) << "\n";
	

	header.dirty = false;
	block_size = header.bl_size;
	cout << "Block Size :\t" << header.bl_size << " " << header.used_block << "\n";
	for (int i = 0; i <= header.used_block; ++i){
		int d = get_nth_bit(i);
		if (!d) free_list.push(i);
	}
	//free_list is guaranteed 2 conatin atleast 1 block
	cout << free_list.front() << "\nOut of open()\n";
	return true;
	//fp now oints to position after header
}
bool file::unread_block(blk_addr block_address)
{
	if (page_tbl.count(block_address) <= 0) {
		cerr << "There is no buffer allocated to this block no : "   << block_address << "\n";
		return false;
	}
	page_tbl[block_address].unread = true;
	unread_pages.insert(block_address);
	return true;
}
bool file::wite_back (blk_addr block_address)
{
	if (page_tbl.count(block_address) <= 0) {
		cerr << "There is no buffer allocated to this block no : "   << block_address << "\n";
		return false;
	}
	page_tbl[block_address].dirty = true;
	dirty_pages.insert(block_address);
	return true;
}
bool file::write_block(blk_addr block_address,void *buf)		//dis fn only writes d block d calling fn shd take care of setting/ resetting d flags
{	//since dis is a generic fn called even if dis is a new block or was read earlier
	if (!get_nth_bit(block_address)){
		cerr << "Block not yet allocated\n";
		return false;
	}
	fseek(fp, _offset + max_bytes + 3 * sizeof(int) + block_size * block_address - ftell(fp), SEEK_CUR);
	//FILE *temp
	int w = fwrite(buf, sizeof(char), block_size, fp);
	cout << "No of bytes written : " << w << "\n";
	if (w < block_size) {
		deallocate_block(block_address);
		return false;
	}
	else{
		return true;	
	} 
}
/*
i cannot remove non-dirty buffer as long as user has read it bcoz user might later expect 2 modify dat buffer but if he has unread it,
it means if he needs it again, he will read it agin
*/
bool file::free_page(char **buffer)
{
	for (iset set_iter = unread_pages.begin(); set_iter != unread_pages.end(); ++set_iter){	
		*buffer = (char *)page_tbl[*set_iter].buf;
		if (page_tbl[*set_iter].dirty){
			page_tbl[*set_iter].dirty = false;
			dirty_pages.erase(*set_iter);
			write_block(*set_iter, *buffer);	
		}
		page_tbl.erase(*set_iter);
		return true;
	}
	return false;
}
void *file::read_block(blk_addr block_address)		//block_address is zero based indexed		returns a char buffer
{
	if (!get_nth_bit(block_address)){
		cerr << "Block not yet allocated\n";
		return false;
	}
	if (page_tbl.count(block_address) > 0) {

		if (page_tbl[block_address].unread) {		//could hav used erase wout checking but dis is more optimal
			unread_pages.erase(block_address);			
		}

		page_tbl[block_address].unread = false;		//since u hav read d bock, u might wanna perform sm operation, hence I shd not remove it
		return page_tbl[block_address].buf;
	}
	char *buf;
	if (page_tbl.size() >= MAX_FRAMES) {							
		if (!free_page(&buf)) { cerr << "Insufficient memory\n"; return NULL; }
	}
	else {
		buf = new char[block_size];
		if (!buf){
			cerr << "Insufficient memory\n";
			return NULL;
		}
	}
	cout << "Reached here\n";
	cout << _offset + max_bytes + 3 * sizeof(int) + block_size * block_address << "\t" << ftell(fp) << "\n";
	fseek(fp,  _offset + max_bytes + 3 * sizeof(int) + block_size * block_address - ftell(fp), SEEK_CUR);
	cout << ftell(fp) << "\n";
	fread(buf, sizeof(char), block_size, fp);
	page_tbl[block_address] = tuple(buf, false, false);
	return buf;
}
void file::delete_file(const char *fname)
{
	////reset variables of object
	fclose(fp);
	fp = NULL;
	string n = "rm ";
	n += string(fname);
	system(n.c_str());
}
bool file::deallocate_block(blk_addr block_address)
{
	if (!get_nth_bit(block_address)){
		cerr << "The block is not yet allocated!!\n";
		return false;
	}
	clear_nth_bit(block_address);
	header.dirty = true;
	free_list.push(block_address);
	return true;
}
int file::allocate_block()		//append a memory chunk equal to bl_size
{
	int i = free_list.front();		//deciding which block 2 allocate
	free_list.pop();
	//free_list conatins header.used_block initially, hence it is either dere in free_list or if it is not dere => it was allocated 2 some blk
	//hence if free_list.size() = 0 => used_block is already allocated hence push used_block + 1
	if (free_list.size() == 0) free_list.push(header.used_block + 1);
	cout << i << " " << header.used_block << "\n";
	if (i < header.used_block) {					//some block in d file is now free
		set_nth_bit(i);
		header.dirty = true;
		return i;
	}
	//now I will need 2 append a block 2 d file

	fseek(fp, 0, SEEK_END);
	cout << ftell(fp) << "\n";
	
	if (header.used_block >= max_block){	
		cerr << "File already is of MAX_SIZE!!\n";
		return -1;
	}
	cout << i << "\n";
	fseek(fp, block_size - 1, SEEK_END);
	char c = '\0';
	fwrite(&c, sizeof(char), 1, fp);
	cout << ftell(fp) << "\n";
	header.dirty = true;
	set_nth_bit(header.used_block);
	//cout << header.used_block << "\n";
	++header.used_block;
	return header.used_block - 1 ;
}
int main()
{
	//file f("test/a.bin", 1024, 0);
	file f("test/a.bin");
	if (!f.success){
		return 1;
	}
	cout << "Hi\n";
	int index = 2;
	/*int index = f.allocate_block();
	if (index == -1) {
		return -1;
	}
	cout << "Block allocated : " << index << "\n";
	if (f.deallocate_block(index))
		cout << "Block successfully deallocated\n";
		*/
	char buf[1024];
	for (int i = 0; i < 1024; ++i)
	{
		buf[i] = '2';
	}
	f.write_block(index, (void *)buf);
	char *s = (char *)f.read_block(index);
	for (int i = 0; i < 1024; ++i){
		cout << s[i] << "\n";
	}
	//f.create();
	//f.write_block()
	return 0;
}