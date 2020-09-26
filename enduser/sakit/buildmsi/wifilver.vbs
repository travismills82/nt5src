' Windows Installer utility to report or update file versions, sizes, languages
' For use with Windows Scripting Host, CScript.exe or WScript.exe
' Copyright (c) 1999-2000, Microsoft Corporation
' Demonstrates the access to install engine and actions
'
Option Explicit
' FileSystemObject.CreateTextFile and FileSystemObject.OpenTextFile
Const OpenAsASCII   = 0 
Const OpenAsUnicode = -1

' FileSystemObject.CreateTextFile
Const OverwriteIfExist = -1
Const FailIfExist      = 0

' FileSystemObject.OpenTextFile
Const OpenAsDefault    = -2
Const CreateIfNotExist = -1
Const FailIfNotExist   = 0
Const ForReading = 1
Const ForWriting = 2
Const ForAppending = 8

Const msiOpenDatabaseModeReadOnly = 0
Const msiOpenDatabaseModeTransact = 1

Const msiViewModifyInsert         = 1
Const msiViewModifyUpdate         = 2
Const msiViewModifyAssign         = 3
Const msiViewModifyReplace        = 4
Const msiViewModifyDelete         = 6

Const msiUILevelNone = 2

Const msiRunModeSourceShortNames = 9

Const msidbFileAttributesNoncompressed = &h00002000

Dim argCount:argCount = Wscript.Arguments.Count
Dim iArg:iArg = 0
If (argCount < 2) Then
	Wscript.Echo "Windows Installer utility to updata File table sizes and versions" &_
		vbNewLine & " The 1st argument is the path to MSI database, at the source file root" &_
		vbNewLine & " The 2nd argument specifies the source file directory" &_
		vbNewLine & " The following options may be specified at any point on the command line" &_
		vbNewLine & "  /U to update the MSI database with the file sizes, versions, and languages" &_
		vbNewLine & " Notes:" &_
		vbNewLine & "  If source type set to compressed, all files will be opened at the root" &_
		vbNewLine & "  Using CSCRIPT.EXE without the /U option, the file info will be displayed" &_
		vbNewLine &_
		vbNewLine & "Copyright (C) Microsoft Corporation, 1999-2000.  All rights reserved."
	Wscript.Quit 1
End If

' Get argument values, processing any option flags
Dim updateMsi    : updateMsi    = False
Dim sequenceFile : sequenceFile = False
Dim databasePath : databasePath = NextArgument
Dim sourceFolder : sourceFolder = NextArgument
If Not IsEmpty(NextArgument) Then Fail "More than 2 arguments supplied" ' process any trailing options
If Not IsEmpty(sourceFolder) And Right(sourceFolder, 1) <> "\" Then sourceFolder = sourceFolder & "\"
Dim console : If UCase(Mid(Wscript.FullName, Len(Wscript.Path) + 2, 1)) = "C" Then console = True

' Connect to Windows Installer object
On Error Resume Next
Dim installer : Set installer = Nothing
Set installer = Wscript.CreateObject("WindowsInstaller.Installer") : CheckError

' Check if multiple language package, and force use of primary language
REM	Set sumInfo = database.SummaryInformation(3) : CheckError

' Open database
Dim database, openMode, view, record, updateMode, sumInfo
If updateMsi Then openMode = msiOpenDatabaseModeTransact Else openMode = msiOpenDatabaseModeReadOnly
Set database = installer.OpenDatabase(databasePath, openMode) : CheckError

' Create an install session and execute actions in order to perform directory resolution
installer.UILevel = msiUILevelNone
Dim session : Set session = installer.OpenPackage(database,1) : If Err <> 0 Then Fail "Database: " & databasePath & ". Invalid installer package format"
Dim shortNames : shortNames = session.Mode(msiRunModeSourceShortNames) : CheckError
'REM If Not IsEmpty(sourceFolder) Then session.Property("OriginalDatabase") = sourceFolder : CheckError
Dim stat : stat = session.DoAction("CostInitialize") : CheckError
If stat <> 1 Then Fail "CostInitialize failed, returned " & stat

Set view = database.OpenView("SELECT File,FileSize,Version,Language,Sequence FROM File ORDER BY File") : CheckError
view.Execute : CheckError

wscript.echo "Updating File properties in the MSI"
Dim objFS : Set objFS=WScript.CreateObject ("Scripting.FileSystemObject")
Dim cabFileFolder : Set cabFileFolder=objFS.GetFolder(sourceFolder) : CheckError
Dim g_cabFileList : Set g_cabFileList = cabFileFolder.Files : CheckError
Dim objFile, nIndex
Dim g_cabFileArray(600)
If (g_cabFileList.Count > 600) Then Fail "Need to increase max file count constant beyond 600"
nIndex = 1
'
' Fill the array of files from the collection.  The binary search for
' the files relies on the fact that this collection is alphabetized already.
'
For Each objFile In g_cabFileList
    Set g_cabFileArray(nIndex) = objFile
    nIndex = nIndex + 1
Next

' Fetch each file and request the source path, then verify the source path, and get the file info if present
Dim fileKey, sourcePath, fileSize, version, language, message, info, nFileCount, nLastIndex
nLastIndex = Round(g_cabFileList.Count / 2)
nFileCount = 0
Do
    nFileCount = nFileCount + 1
    Set record = view.Fetch : CheckError
    If record Is Nothing Then Exit Do   
    fileKey    = record.StringData(1)
    REM	fileSize   = record.IntegerData(2)
    REM	version    = record.StringData(3)
    REM	language   = record.StringData(4)
    REM sequence   = record.StringData(5)
    'wscript.echo nFileCount & ": " & fileKey
    
	sourcePath = sourceFolder & fileKey
	If installer.FileAttributes(sourcePath) = -1 Then
		message = message & vbNewLine & sourcePath
	Else
		fileSize = installer.FileSize(sourcePath) : CheckError
		version  = Empty : version  = installer.FileVersion(sourcePath, False) : Err.Clear ' early MSI implementation fails if no version
		language = Empty : language = installer.FileVersion(sourcePath, True)  : Err.Clear ' early MSI implementation doesn't support language
		If language = version Then language = Empty ' Temp check for MSI.DLL version without language support
		If Err <> 0 Then version = Empty : Err.Clear
		If updateMsi Then
			record.IntegerData(2) = fileSize
			If Len(version)  > 0 Then record.StringData(3) = version
			If Len(language) > 0 Then record.StringData(4) = language
			nIndex = GetFileIndex(fileKey, nLastIndex)
			record.IntegerData(5) = nIndex
			view.Modify msiViewModifyUpdate, record : CheckError
			nLastIndex = nIndex + 1
		ElseIf console Then
			info = fileName : If Len(info) < 12 Then info = info & Space(12 - Len(info))
			info = info & "  size=" & fileSize : If Len(info) < 26 Then info = info & Space(26 - Len(info))
			If Len(version)  > 0 Then info = info & "  vers=" & version : If Len(info) < 45 Then info = info & Space(45 - Len(info))
			If Len(language) > 0 Then info = info & "  lang=" & language
			Wscript.Echo info
		End If
	End If
Loop
REM Wscript.Echo "SourceDir = " & session.Property("SourceDir")
If Not IsEmpty(message) Then Fail "Error, the following files were not available:" & message

' Update SummaryInformation
If updateMsi Then
	Set sumInfo = database.SummaryInformation(3) : CheckError
	sumInfo.Property(11) = Now
	sumInfo.Property(13) = Now
	sumInfo.Persist
End If

' Commit database in case updates performed
database.Commit : CheckError
Wscript.Quit 0


'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
' GetFileIndex
'
' Description
'   Performs a binary search for a file in an alphabetized list so that 
'   the sequence number can be changed to match the index of the file 
'   in the CAB.
'
' Parameters
'   strFilename   Filename to search for
'   nIndex        A best guess of where to start looking
'                 in the array for the filename
'
' Returns
'   The index of the file in the list
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
Function GetFileIndex(strFilename, nIndx)
    Dim nMin, nMax, nRound, nCompare
    '
    ' nMin and nMax are the inclusive boundaries where the file can be found 
    ' in the array
    '
    nMin = 1
    nMax = g_cabFileList.Count
    
    If (nIndx > nMax) OR (nIndx < nMin) Then nIndx = Round(nMax / 2)
    nRound = 1
    
    Do
        'wscript.echo "Entering loop: min=" & nMin & " max=" & nMax & " current=" & nIndx
        nCompare = StrComp(strFilename, g_cabFileArray(nIndx).Name, 1)
        If nCompare = 0 Then
            Exit Do 'Found the string
        ElseIf nCompare < 0 Then
            nMax = nIndx - 1 'The string is in the lower half of the bounds
        Else
            nMin = nIndx + 1 'The string is in the upper half of the bounds
        End If
        nIndx = Round((nMax + nMin) / 2)
        
        If nMax < nMin Then Fail "ERROR: Could not find file " & strFilename & " in CAB directory. Files may not be sorted properly"
        nRound = nRound + 1
    Loop
        
   	'wscript.echo "Index for " & strFilename & " is " & nIndx & " (" & nRound & " tries)"
   	GetFileIndex = nIndx
End Function

' Extract argument value from command line, processing any option flags
Function NextArgument
	Dim arg
	Do  ' loop to pull in option flags until an argument value is found
		If iArg >= argCount Then Exit Function
		arg = Wscript.Arguments(iArg)
		iArg = iArg + 1
		If (AscW(arg) <> AscW("/")) And (AscW(arg) <> AscW("-")) Then Exit Do
		Select Case UCase(Right(arg, Len(arg)-1))
			Case "U" : updateMsi    = True
			Case Else: Wscript.Echo "Invalid option flag:", arg : Wscript.Quit 1
		End Select
	Loop
	NextArgument = arg
End Function

Sub CheckError
	Dim message, errRec
	If Err = 0 Then Exit Sub
	message = Err.Source & " " & Hex(Err) & ": " & Err.Description & ", " & Err.number
	If Not installer Is Nothing Then
		Set errRec = installer.LastErrorRecord
		If Not errRec Is Nothing Then message = message & vbNewLine & errRec.FormatText
	End If
	Fail message
End Sub

Sub Fail(message)
	Wscript.Echo message
	Wscript.Quit 2
End Sub
