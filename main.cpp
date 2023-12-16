#include <Windows.h>
#include <aclapi.h>
#include <tchar.h>
#include <iostream>

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	STARTUPINFO			dispatcherStartupInfo{ };	// Startup information for prowin32
	PROCESS_INFORMATION dispatcherProcessInfo{ };	// Process information for prowin32
	DWORD				processStatus{ 1 };
	SECURITY_ATTRIBUTES securityAttributes{ };
	HANDLE				jobProcess{ };
	SECURITY_DESCRIPTOR jobDescriptor{ }; 
	EXPLICIT_ACCESS		ea;
	PACL				acl;
	DWORD				fResponse = ERROR_SUCCESS;
	JOBOBJECT_BASIC_PROCESS_ID_LIST currentJobProcesses;
	JOBOBJECT_CPU_RATE_CONTROL_INFORMATION jobCpuControlRateInformation;

	/**
	* We're going to start by initializing all of our data structures. We need to zero their memory to make
	* sure we're starting from a clean state. After that, we're going to create a security descriptor an 
	* Job Object. This security descriptor give full process permission for the process but only if the
	* process is ran as an administrator. When the Job Object has been made, we fill in a 
	* JOBOBJECT_CPU_RATE_CONTROL_INFORMATION with the data needed to limit the CPU rate of the Job Object
	* to 10% of the CPU rate of the system.
	*/

	// Zero startup, security, and process info data structures.
	ZeroMemory(&dispatcherStartupInfo, sizeof(STARTUPINFO));
	ZeroMemory(&dispatcherProcessInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&securityAttributes, sizeof(SECURITY_ATTRIBUTES));
	ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
	ZeroMemory(&acl, sizeof(PACL));
	ZeroMemory(&currentJobProcesses, sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST));
	ZeroMemory(&jobCpuControlRateInformation, sizeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION));

	// Mark data size of startup info structure.
	dispatcherStartupInfo.cb = sizeof dispatcherStartupInfo;

	// Assign all access permissions to the ACE to be assigned to "jobProcess". Allow only the 
	// "Administrators" user group to execute.
	ea.grfAccessPermissions = JOB_OBJECT_ALL_ACCESS;
	ea.grfAccessMode		= GRANT_ACCESS;
	ea.grfInheritance		= NO_INHERITANCE;
	ea.Trustee.TrusteeForm	= TRUSTEE_IS_NAME;
	ea.Trustee.TrusteeType	= TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea.Trustee.ptstrName	= (LPWSTR)L"Everyone";

	// Initialize our security descriptor.
	if (InitializeSecurityDescriptor(&jobDescriptor, SECURITY_DESCRIPTOR_REVISION) == 0) {
		std::cout << GetLastError();
		return 200;
	}

	// Assign ACE entry to Acl.
	fResponse = SetEntriesInAcl(1, &ea, nullptr, &acl);
	if (fResponse != ERROR_SUCCESS) {
		std::cout << "Unable to assign ACE entry to Acl. " << GetLastError();
		return 200;
	}
	
	// Assign the Acl created in "acl" to the "jobDescriptor" security descriptor.
	fResponse = SetSecurityDescriptorDacl(&jobDescriptor, TRUE, acl, FALSE);

	if (fResponse == 0) {
		std::cout << "Unable to set security descriptor Acl.  " << GetLastError();
		return 200;
	}

	// Assign our security attributes.
	securityAttributes.nLength				= sizeof(SECURITY_ATTRIBUTES);
	securityAttributes.lpSecurityDescriptor = &jobDescriptor;
	securityAttributes.bInheritHandle		= FALSE;

	// Attempt to create our job object.
	jobProcess = CreateJobObjectW(&securityAttributes, (LPCWSTR)L"prowin32controljob");

	if (jobProcess == NULL) {
		std::cout << "Failed to create job process. " << GetLastError();
		return 200;
	}

	// Set and assign our base CPU control rate information to prevent excessive CPU use.
	jobCpuControlRateInformation.ControlFlags = 
		JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
	jobCpuControlRateInformation.CpuRate = 1000; // 10 percent CPU rate.

	fResponse = SetInformationJobObject(jobProcess,
		JobObjectCpuRateControlInformation,
		&jobCpuControlRateInformation,
		sizeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION));

	if (!fResponse) {
		std::cout << "Unable to set CPU control rate. " << GetLastError();
	}

	/**
	* Now that we're finished creating our job object within the specifications we need, we're going to start
	* creating our processes and assigning them to the job. We start with stopping message dispatcher.
	*/

	// Create our handle to stopappl.exe
	if (!CreateProcessW(
		(LPWSTR)L"D:/stopappl.exe",
		(LPWSTR)L" -backoffice", // Only stop back office
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED | CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS,
		NULL,
		(LPWSTR)L"D:/",
		&dispatcherStartupInfo,
		&dispatcherProcessInfo
	)) {
		std::cout << "Couldn't create process. " << GetLastError();
		return 100;
	}

	// Try to assign process to our job object.
	fResponse = AssignProcessToJobObject(jobProcess, dispatcherProcessInfo.hProcess);

	if (fResponse == 0) {
		std::cout << "Unable to assign process to job object. " << GetLastError();
		CloseHandle(jobProcess);
		CloseHandle(dispatcherProcessInfo.hThread);
		CloseHandle(dispatcherProcessInfo.hProcess);
		return 300;
	}

	// Resume the application stop process.
	ResumeThread(dispatcherProcessInfo.hThread);

	// Query the number of running processes. We will continue to check until there are no process IDs left
	// in the job object. 
	do {
		if ((QueryInformationJobObject(jobProcess,
			JobObjectBasicProcessIdList,
			&currentJobProcesses,
			sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST),
			NULL) == NULL) && GetLastError() != ERROR_MORE_DATA) {
			std::cout << "Unable to get Job Process information. " << GetLastError();
			TerminateJobObject(jobProcess, 4);
			CloseHandle(jobProcess);
			CloseHandle(dispatcherProcessInfo.hThread);
			CloseHandle(dispatcherProcessInfo.hProcess);
			return 300;
		}
	} while (currentJobProcesses.NumberOfProcessIdsInList > 0);

	// Once message dispatcher has stopped, close the handles for dispatcherProcessInfo so we can reassign the
	// structure to a new process. After that, we create the new process with the CREATE_SUSPENDED flag so 
	// it doesn't run before we can assign it to the job. Then, we assign it to the job.

	CloseHandle(dispatcherProcessInfo.hThread);
	CloseHandle(dispatcherProcessInfo.hProcess);

	if (!CreateProcessW((LPCWSTR)L"D:/Progress/OpenEdge/bin/prowin32.exe",
		(LPWSTR)L" -p D:/mi9store/obj/s3/strtbackoff.p",
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED | CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS,
		NULL,
		(LPCWSTR)L"D:/",
		&dispatcherStartupInfo,
		&dispatcherProcessInfo)) {
		std::cout << "Couldn't create process. " << GetLastError();
		return 100;
	}

	fResponse = AssignProcessToJobObject(jobProcess, dispatcherProcessInfo.hProcess);

	if (fResponse == 0) {
		std::cout << "Unable to assign process to job object. " << GetLastError();
		CloseHandle(jobProcess);
		CloseHandle(dispatcherProcessInfo.hThread);
		CloseHandle(dispatcherProcessInfo.hProcess);
		return 300;
	}

	// Sleep for a couple of seconds after the new process is assigned.
	Sleep(2000);

	// Start the thread back up for the new process. This time, instead of querying *while* the process ID
	// list length is greater than 0, we're going to query *until* it's greater than 0. We do this because
	// the back office startup is going to create many child processes while things start up, and we want to
	// make sure the child processes come back up.

	ResumeThread(dispatcherProcessInfo.hThread);

	do {
		if ((QueryInformationJobObject(jobProcess,
			JobObjectBasicProcessIdList,
			&currentJobProcesses,
			sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST),
			NULL) == NULL) && GetLastError() != ERROR_MORE_DATA) {
			std::cout << "Unable to get Job Process information. " << GetLastError();
			TerminateJobObject(jobProcess, 4);
			CloseHandle(jobProcess);
			CloseHandle(dispatcherProcessInfo.hThread);
			CloseHandle(dispatcherProcessInfo.hProcess);
			return 300;
		}
	} while (currentJobProcesses.NumberOfProcessIdsInList < 1);


	// From here, the restart was completed successfully. We're going to clean up all of our handles.

	CloseHandle(jobProcess);
	CloseHandle(dispatcherProcessInfo.hThread);
	CloseHandle(dispatcherProcessInfo.hProcess);
	return 0;
}