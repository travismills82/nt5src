//------------------------------------------------------------------------------
// <copyright file="GroupBoxDesigner.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>                                                                
//------------------------------------------------------------------------------

/*
 */

//#define TESTVALUEUI
namespace System.Windows.Forms.Design {
    

    using System.Diagnostics;

    using System;
    using System.Design;
    using System.Windows.Forms;
    using System.Drawing;
    using Microsoft.Win32;
    
    using System.ComponentModel;
    using System.Drawing.Design;
    using System.ComponentModel.Design;
    using System.Collections;

    /// <include file='doc\GroupBoxDesigner.uex' path='docs/doc[@for="GroupBoxDesigner"]/*' />
    /// <devdoc>
    ///     This class handles all design time behavior for the group box class.  Group
    ///     boxes may contain sub-components and therefore use the frame designer.
    /// </devdoc>
    [System.Security.Permissions.SecurityPermission(System.Security.Permissions.SecurityAction.Demand, Flags=System.Security.Permissions.SecurityPermissionFlag.UnmanagedCode)]
    internal class GroupBoxDesigner : ParentControlDesigner {
    
        private InheritanceUI inheritanceUI;
        
        /// <include file='doc\GroupBoxDesigner.uex' path='docs/doc[@for="GroupBoxDesigner.DefaultControlLocation"]/*' />
        /// <devdoc>
        /// Determines the default location for a control added to this designer.
        /// it is usualy (0,0), but may be modified if the container has special borders, etc.
        /// </devdoc>
        protected override Point DefaultControlLocation {
            get {
                GroupBox gb = (GroupBox)this.Control;
                return new Point(gb.DisplayRectangle.X,gb.DisplayRectangle.Y);
            }
        }
    
       #if TESTVALUEUI
        /// <include file='doc\GroupBoxDesigner.uex' path='docs/doc[@for="GroupBoxDesigner.Initialize"]/*' />
        /// <devdoc>
        ///     Initializes the designer with the given component.  The designer can
        ///     get the component's site and request services from it in this call.
        /// </devdoc>
        public override void Initialize(IComponent component) {
            base.Initialize(component);
            
            
            
            IPropertyValueUIService pvUISvc = (IPropertyValueUIService)component.Site.GetService(typeof(IPropertyValueUIService));
            
            if (pvUISvc != null) {
                pvUISvc.AddPropertyValueUIHandler(new PropertyValueUIHandler(this.OnGetUIValueItem));
            }
        }
        
        private void OnGetUIValueItem(object component, PropertyDescriptor propDesc, ArrayList valueUIItemList){
        
            if (propDesc.PropertyType == typeof(string)) {
                Bitmap bmp = new Bitmap(typeof(GroupBoxDesigner), "BoundProperty.bmp");
                bmp.MakeTransparent();
                valueUIItemList.Add(new LocalUIItem(bmp, new PropertyValueUIItemInvokeHandler(this.OnPropertyValueUIItemInvoke), "Data Can"));
                
                //bmp = new Bitmap("BoundProperty.bmp");
                valueUIItemList.Add(new LocalUIItem(bmp, new PropertyValueUIItemInvokeHandler(this.OnPropertyValueUIItemInvoke), "Little Button"));
            }
            
            
        }

        private void OnPropertyValueUIItemInvoke(ITypeDescriptorContext context, PropertyDescriptor descriptor, PropertyValueUIItem invokedItem) {
            Debug.Fail("propertyuivalue '" + invokedItem.ToolTip + "' invoked");
        }
        
        #endif
      
        /// <include file='doc\GroupBoxDesigner.uex' path='docs/doc[@for="GroupBoxDesigner.OnPaintAdornments"]/*' />
        /// <devdoc>
        ///      We override this because even though we still want to 
        ///      offset our grid for our display rectangle, we still want
        ///      to align to our parent's grid - so we don't look funny
        /// </devdoc>
        protected override void OnPaintAdornments(PaintEventArgs pe) {
            if (DrawGrid) {
                Control control = (Control)Control;
                Rectangle rectangle = Control.DisplayRectangle;
                
                rectangle.Width++; // gpr: FillRectangle with a TextureBrush comes up one pixel short
                rectangle.Height++;
                ControlPaint.DrawGrid(pe.Graphics, rectangle, GridSize, control.BackColor);
            }
            
            // If this control is being inherited, paint it
            //
            if (Inherited) {
                if (inheritanceUI == null) {
                    inheritanceUI = (InheritanceUI)GetService(typeof(InheritanceUI));
                }
                
                if (inheritanceUI != null) {
                    pe.Graphics.DrawImage(inheritanceUI.InheritanceGlyph, 0, 0);
                }
            }
        }

        /// <include file='doc\GroupBoxDesigner.uex' path='docs/doc[@for="GroupBoxDesigner.WndProc"]/*' />
        /// <devdoc>
        ///      We override our base class's WndProc here because
        ///      the group box always returns HTTRANSPARENT.  This
        ///      causes the mouse to go "through" the group box, but
        ///      that's not what we want at design time.
        /// </devdoc>
        protected override void WndProc(ref Message m) {
            switch (m.Msg) {
                case NativeMethods.WM_NCHITTEST:
                    // The group box always fires HTTRANSPARENT, which
                    // causes the message to go to our parent.  We want
                    // the group box's designer to get these messages, however,
                    // so change this.
                    //
                    base.WndProc(ref m);
                    if ((int)m.Result == NativeMethods.HTTRANSPARENT) {
                        m.Result = (IntPtr)NativeMethods.HTCLIENT;
                    }
                    break;

                default:
                    base.WndProc(ref m);
                    break;
            }
        }
        
        
        #if TESTVALUEUI
        
        internal class LocalUIItem : PropertyValueUIItem {
            private string itemName;
            
            public LocalUIItem(Image img, PropertyValueUIItemInvokeHandler handler, string itemName) : base(img, handler, itemName) {
                this.itemName = itemName;
            }
        }
        #endif
    }
}

