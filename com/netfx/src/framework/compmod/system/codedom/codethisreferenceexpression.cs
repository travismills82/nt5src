//------------------------------------------------------------------------------
// <copyright file="CodeThisReferenceExpression.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>                                                                
//------------------------------------------------------------------------------

namespace System.CodeDom {

    using System.Diagnostics;
    using System;
    using Microsoft.Win32;
    using System.Collections;
    using System.Runtime.InteropServices;

    /// <include file='doc\CodeThisReferenceExpression.uex' path='docs/doc[@for="CodeThisReferenceExpression"]/*' />
    /// <devdoc>
    ///    <para>
    ///       Represents a current instance reference.
    ///    </para>
    /// </devdoc>
    [
        ClassInterface(ClassInterfaceType.AutoDispatch),
        ComVisible(true),
        Serializable,
    ]
    public class CodeThisReferenceExpression : CodeExpression {
    }
}
