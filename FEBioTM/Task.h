#pragma once
#include <FL/Fl_Text_Display.H>

class Progress;
class Timer;

//-----------------------------------------------------------------------------
// The task defines a problem to be run with FEBio. It has several states:
// - READY = File is read in and awaits to be scheduled.
// - MODIFIED = File has been modified. Will be saved automatically when run.
// - QUEUED = File is scheduled to be run.
// - RUNNING = File is being run.
// - COMPLETED = File has run and terminated successfully.
// - FAILED = File has run but did not terminate successfully.
// - CANCELLED = Run was cancelled by the user before terminating.
//
// Each task also manages its own input file through the Fl_Text_Buffer
// reference. 
//
// Only one task can be run at the same time. This limitation must be
// imposed because there is only one output window where the output from
// the run will be directed to. The task that is currently running is
// store in the CTask::m_prun member.
//
class CTask
{
	// max file name
	enum {MAX_FILE = 512};

public:
	struct STATS
	{
		int		nreturn;	// return value
		int		ntime;		// number of time steps
		int		niters;		// number of iterations
		int		nrhs;		// number of RHS evaluations
		int		nreform;	// number of reformations
		
		// run time params
		int		nhour;
		int		nmin;
		int		nsec;
	};

public:
	// status values
	// note that at any given time, only one task may have its status set to RUNNING
	enum { READY, MODIFIED, QUEUED, RUNNING, COMPLETED, FAILED, CANCELLED };

public:
	// con-/de-structor
	CTask();
	~CTask();

	// file name handling
	void SetFileName(const char* szfile);
	const char* GetFileName() { return m_szfile; }
	const char* GetFileTitle();
	void GetFilePath(char* sz);

	// get/set the input file text buffer
	void SetTextBuffer(Fl_Text_Buffer* pb);
	Fl_Text_Buffer* GetTextBuffer() { return m_pFile; }
	Fl_Text_Buffer* GetStyleBuffer() { return m_pStyle; }

	// get/set the task status
	void SetStatus(int n) { m_nstatus = n; }
	int GetStatus() { return m_nstatus; }

	int FileSize() { return m_pFile->length(); }

	// save the task input file
	void Save() { if (m_nstatus == MODIFIED) { m_pFile->savefile(m_szfile); SetStatus(READY);} }
	void Save(const char* szfile) { SetFileName(szfile); Save(); }

	// reload the file
	void Revert();

	// run this task
	void Run(Progress& prg);
	static CTask* GetRunningTask() { return m_prun; }

	void UpdateRunTime();

public:
	void UpdateStyle(int pos, int nInserted, int nDeleted, int nRestyled, const char* deletedText);

protected:
	char			m_szfile[MAX_FILE];		//!< file name
	Fl_Text_Buffer*	m_pFile;				//!< text buffer for editing
	Fl_Text_Buffer*	m_pStyle;				//!< text buffer for styling
	int				m_nstatus;				//!< status

public: // FEBio command line and control options
	bool	m_bdebug;	//!< debug mode
	int		m_nlog;		//!< log level

	STATS	m_stats;	//!< the stats
	Timer*	m_ptime;

private:
	static CTask*	m_prun;	// this is the task that is running
};
