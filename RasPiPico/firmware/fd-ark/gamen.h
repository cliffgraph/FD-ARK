#pragma once

struct GAMENINFO
{
	enum class GAMENNO
	{
		NONE = -1,
		CTRLFD_TOP_MENU	 = 0,
		CTRLFD_READ_FD	 = 1,
		CTRLFD_WRITE_FD	 = 2,
		EMU_TOP_MENU	 = 3,
	};

	GAMENNO GamenNo, GamenNoOld;
	int SelectedFdNo;
	GAMENINFO() : GamenNo(GAMENNO::NONE), GamenNoOld(GAMENNO::NONE), SelectedFdNo(0) {}
};

class IGamen
{
public:
	virtual ~IGamen() {return;}
	virtual void Init(GAMENINFO *pG) = 0;
	virtual void Start() = 0;
	virtual void Main() = 0;
};
