// ==++==
// 
//   Copyright (c) Microsoft Corporation.  All rights reserved.
// 
// ==--==
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
namespace System.Runtime.InteropServices {
	using System;
    // Use this in P/Direct function prototypes to specify 
    // which character set to use when marshalling Strings.
    // Using Ansi will marshal the strings as 1 byte char*'s.
    // Using Unicode will marshal the strings as 2 byte wchar*'s.
    // Generally you probably want to use Auto, which does the
    // right thing 99% of the time.
    /// <include file='doc\CharSet.uex' path='docs/doc[@for="CharSet"]/*' />
    [Serializable] 
	public enum CharSet
    {
    	/// <include file='doc\CharSet.uex' path='docs/doc[@for="CharSet.None"]/*' />
    	None = 1,		// User didn't specify how to marshal strings.
    	/// <include file='doc\CharSet.uex' path='docs/doc[@for="CharSet.Ansi"]/*' />
    	Ansi = 2,		// Strings should be marshalled as ANSI 1 byte chars. 
    	/// <include file='doc\CharSet.uex' path='docs/doc[@for="CharSet.Unicode"]/*' />
    	Unicode = 3,    // Strings should be marshalled as Unicode 2 byte chars.
    	/// <include file='doc\CharSet.uex' path='docs/doc[@for="CharSet.Auto"]/*' />
    	Auto = 4,		// Marshal Strings in the right way for the target system. 
    }
}
