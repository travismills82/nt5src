//------------------------------------------------------------------------------
// <copyright file="ValidationEventArgs.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>                                                                
//------------------------------------------------------------------------------

namespace System.Xml.Schema {
    /// <include file='doc\ValidationEventArgs.uex' path='docs/doc[@for="ValidationEventArgs"]/*' />
    /// <devdoc>
    ///    Returns detailed information relating to
    ///    the ValidationEventhandler.
    /// </devdoc>
    public sealed class ValidationEventArgs : EventArgs {
        XmlSchemaException ex;
        XmlSeverityType severity;

        internal ValidationEventArgs( XmlSchemaException ex ) : base() {
            this.ex = ex; 
            severity = XmlSeverityType.Error;
        }
 
        internal ValidationEventArgs( XmlSchemaException ex , XmlSeverityType severity ) : base() {
            this.ex = ex; 
            this.severity = severity;
        }

        //UE Attention
        /// <include file='doc\ValidationEventArgs.uex' path='docs/doc[@for="ValidationEventArgs.Severity"]/*' />
        public XmlSeverityType Severity {
            get { return severity;}
        }

        //UE Attention
        /// <include file='doc\ValidationEventArgs.uex' path='docs/doc[@for="ValidationEventArgs.Exception"]/*' />
        public XmlSchemaException Exception {
            get { return ex;}
        }

        /// <include file='doc\ValidationEventArgs.uex' path='docs/doc[@for="ValidationEventArgs.Message"]/*' />
        /// <devdoc>
        ///    <para>Gets the text description corresponding to the
        ///       validation error.</para>
        /// </devdoc>
        public String Message {
            get { return ex.Message;}
        }
    }
}
