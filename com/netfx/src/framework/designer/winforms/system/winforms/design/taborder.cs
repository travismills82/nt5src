//------------------------------------------------------------------------------
// <copyright file="TabOrder.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>                                                                
//------------------------------------------------------------------------------

/*
 */
namespace System.Windows.Forms.Design {
    using System;
    using System.Design;
    using System.Collections;
    using System.ComponentModel;
    using System.ComponentModel.Design;
    using Microsoft.Win32;
    using System.Diagnostics;
    using System.Drawing;
    using System.Globalization;
    using System.Runtime.InteropServices;
    using System.Text;
    using System.Windows.Forms;

    /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder"]/*' />
    /// <devdoc>
    ///     This class encapsulates the tab order UI for our form designer.
    /// </devdoc>
    [
    System.Security.Permissions.SecurityPermission(System.Security.Permissions.SecurityAction.Demand, Flags=System.Security.Permissions.SecurityPermissionFlag.UnmanagedCode),
    ToolboxItem(false),
    DesignTimeVisible(false)
    ]
    internal class TabOrder : Control, IMouseHandler, IMenuStatusHandler {
        private IDesignerHost   host;
        private Control         ctlHover;
        private ArrayList      tabControls;
        private Rectangle[]     tabGlyphs;
        private ArrayList      tabComplete;
        private Hashtable       tabNext;
        private Font            tabFont;
        private StringBuilder   drawString;
        private Brush           highlightTextBrush;
        private Pen             highlightPen;
        private int             selSize;
        private Hashtable       tabProperties;
        private Region          region = null;
        private MenuCommand[]   commands;
        private MenuCommand[]   newCommands;
        private string          decimalSep;

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.TabOrder"]/*' />
        /// <devdoc>
        ///     Creates a new tab order control that displays the tab order
        ///     UI for a form.
        /// </devdoc>
        public TabOrder(IDesignerHost host) {
            this.host = host;

            // Determine a font for us to use.
            //
            IUIService uisvc = (IUIService)host.GetService(typeof(IUIService));
            if (uisvc != null) {
                tabFont = (Font)uisvc.Styles["DialogFont"];
            }
            else {
                tabFont = Control.DefaultFont;
            }
            tabFont = new Font(tabFont, FontStyle.Bold);

            // And compute the proper highlight dimensions.
            //
            ISelectionUIService seluisvc = (ISelectionUIService)host.GetService(typeof(ISelectionUIService));
            if (seluisvc != null) {
                selSize = seluisvc.GetAdornmentDimensions(AdornmentType.GrabHandle).Width;
                seluisvc.Visible = false;
            }
            else {
                selSize = 7;
            }

            // Colors and brushes...
            //
            drawString  = new StringBuilder(12);
            highlightTextBrush = new SolidBrush(SystemColors.HighlightText);
            highlightPen = new Pen(SystemColors.Highlight);
            
            // The decimal separator
            //
            NumberFormatInfo formatInfo = (NumberFormatInfo)CultureInfo.CurrentCulture.GetFormat(typeof(NumberFormatInfo));
            if (formatInfo != null) {
                decimalSep = formatInfo.NumberDecimalSeparator;
            }
            else {
                decimalSep = ".";
            }
            

            tabProperties = new Hashtable();

            // Set up a NULL brush so we never try to invalidate the control.  This is
            // more efficient for what we're doing
            //
            SetStyle(ControlStyles.Opaque, true);

            // We're an overlay on top of the form
            //
            IOverlayService os = (IOverlayService)host.GetService(typeof(IOverlayService));
            Debug.Assert(os != null, "No overlay service -- tab order UI cannot be shown");
            if (os != null) {
                os.PushOverlay(this);
            }

            // Push a help keyword so the help system knows we're in place.
            //
            IHelpService hs = (IHelpService)host.GetService(typeof(IHelpService));
            if (hs != null) {
                hs.AddContextAttribute("Keyword", "TabOrderView", HelpKeywordType.FilterKeyword);
            }

            commands = new MenuCommand[] {
                new MenuCommand(new EventHandler(OnKeyCancel),
                                MenuCommands.KeyCancel),

                new MenuCommand(new EventHandler(OnKeyDefault),
                                MenuCommands.KeyDefaultAction),

                new MenuCommand(new EventHandler(OnKeyPrevious),
                                MenuCommands.KeyMoveUp),

                new MenuCommand(new EventHandler(OnKeyNext),
                                MenuCommands.KeyMoveDown),

                new MenuCommand(new EventHandler(OnKeyPrevious),
                                MenuCommands.KeyMoveLeft),

                new MenuCommand(new EventHandler(OnKeyNext),
                                MenuCommands.KeyMoveRight),

                new MenuCommand(new EventHandler(OnKeyNext),
                                MenuCommands.KeySelectNext),

                new MenuCommand(new EventHandler(OnKeyPrevious),
                                MenuCommands.KeySelectPrevious),
            };
            
            newCommands = new MenuCommand[] {
                new MenuCommand(new EventHandler(OnKeyDefault),
                                MenuCommands.KeyTabOrderSelect),
            };
            
            IMenuCommandService mcs = (IMenuCommandService)host.GetService(typeof(IMenuCommandService));
            if (mcs != null) {
                foreach(MenuCommand mc in newCommands) {
                    mcs.AddCommand(mc);
                }
            }

            // We also override keyboard, menu and mouse handlers.  Our override relies on the
            // above array of menu commands, so this must come after we initialize the array.
            //
            IEventHandlerService ehs = (IEventHandlerService)host.GetService(typeof(IEventHandlerService));
            if (ehs != null) {
                ehs.PushHandler(this);
            }
            
            // We sync add, remove and change events so we remain in sync with any nastiness that the
            // form may pull on us.
            //
            IComponentChangeService cs = (IComponentChangeService)host.GetService(typeof(IComponentChangeService));
            if (cs != null) {
                cs.ComponentAdded += new ComponentEventHandler(this.OnComponentAddRemove);
                cs.ComponentRemoved += new ComponentEventHandler(this.OnComponentAddRemove);
                cs.ComponentChanged += new ComponentChangedEventHandler(this.OnComponentChanged);
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.Dispose"]/*' />
        /// <devdoc>
        ///     Called when it is time for the tab order UI to go away.
        /// </devdoc>
        protected override void Dispose(bool disposing) {
            if (disposing) {
                if (region != null) {
                    region.Dispose();
                    region = null;
                }

                if (host != null) {
                    IOverlayService os = (IOverlayService)host.GetService(typeof(IOverlayService));
                    if (os != null) {
                        os.RemoveOverlay(this);
                    }

                    IEventHandlerService ehs = (IEventHandlerService)host.GetService(typeof(IEventHandlerService));
                    if (ehs != null) {
                        ehs.PopHandler(this);
                    }

                    IMenuCommandService mcs = (IMenuCommandService)host.GetService(typeof(IMenuCommandService));
                    if (mcs != null) {
                        foreach(MenuCommand mc in newCommands) {
                            mcs.RemoveCommand(mc);
                        }
                    }

                    ISelectionUIService seluisvc = (ISelectionUIService)host.GetService(typeof(ISelectionUIService));
                    if (seluisvc != null) {
                        seluisvc.Visible = true;
                    }

                    // We sync add, remove and change events so we remain in sync with any nastiness that the
                    // form may pull on us.
                    //
                    IComponentChangeService cs = (IComponentChangeService)host.GetService(typeof(IComponentChangeService));
                    if (cs != null) {
                        cs.ComponentAdded -= new ComponentEventHandler(this.OnComponentAddRemove);
                        cs.ComponentRemoved -= new ComponentEventHandler(this.OnComponentAddRemove);
                        cs.ComponentChanged -= new ComponentChangedEventHandler(this.OnComponentChanged);
                    }

                    IHelpService hs = (IHelpService)host.GetService(typeof(IHelpService));
                    if (hs != null) {
                        hs.RemoveContextAttribute("Keyword", "TabOrderView");
                    }

                    host = null;
                }
            }
            base.Dispose(disposing);
        }
        
        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.DrawTabs"]/*' />
        /// <devdoc>
        ///     this function does double duty:  it draws the tabs if fRegion is false, or it
        ///     computes control region rects if fRegion is true (both require that we essentially
        ///     "draw" the tabs)
        /// </devdoc>
        private void DrawTabs(IList tabs, Graphics gr, bool fRegion) {
            IEnumerator e = tabs.GetEnumerator();
            int         iCtl = 0;
            Control     ctl;
            Control     parent;
            Rectangle   rc = Rectangle.Empty;
            Size        sz = Size.Empty;
            string      str;

            Font font = tabFont;

            if (fRegion) {
                region = new Region(new Rectangle(0, 0, 0, 0));
            }

            if (ctlHover != null) {
                Rectangle ctlInner = GetConvertedBounds(ctlHover);
                Rectangle ctlOuter = ctlInner;
                ctlOuter.Inflate(selSize, selSize);

                if (fRegion) {
                    region = new Region(ctlOuter);
                    region.Exclude(ctlInner);
                }
                else {
                    Control p = ctlHover.Parent;
                    Color backColor;
                    backColor = p.BackColor;

                    Region clip = gr.Clip;
                    gr.ExcludeClip(ctlInner);
                    gr.FillRectangle(new SolidBrush(backColor), ctlOuter);

                    ControlPaint.DrawSelectionFrame(gr, false, ctlOuter, ctlInner, backColor);
                    gr.Clip = clip;
                }
            }

            while (e.MoveNext()) {
                ctl = (Control)e.Current;
                rc = GetConvertedBounds(ctl);

                drawString.Length = 0;

                parent = GetSitedParent(ctl);
                Control baseControl = (Control)host.RootComponent;

                while (parent != baseControl && parent != null) {
                    drawString.Insert(0, decimalSep);
                    drawString.Insert(0, parent.TabIndex.ToString());
                    parent = GetSitedParent(parent);
                }

                drawString.Insert(0, ' ');
                drawString.Append(ctl.TabIndex.ToString());
                drawString.Append(' ');

                if (((PropertyDescriptor)tabProperties[ctl]).IsReadOnly) {
                    drawString.Append(SR.GetString(SR.WindowsFormsTabOrderReadOnly));
                    drawString.Append(' ');
                }

                str = drawString.ToString();
                sz = Size.Ceiling(gr.MeasureString(str, font));
                rc.Width = sz.Width + 2;
                rc.Height = sz.Height + 2;
                
                tabGlyphs[iCtl++] = rc;

                Brush brush;
                Pen pen;
                Color textColor;
                if (fRegion) {
                    region.Union(rc);
                }
                else {
                    if (tabComplete.IndexOf(ctl)!=-1) {
                        brush = highlightTextBrush;
                        pen = highlightPen;
                        textColor = SystemColors.Highlight;
                    }
                    else {
                        brush = SystemBrushes.Highlight;
                        pen = SystemPens.HighlightText;
                        textColor = SystemColors.HighlightText;
                    }

                    gr.FillRectangle(brush, rc);
                    gr.DrawRectangle(pen, rc.X, rc.Y, rc.Width - 1, rc.Height - 1);

                    Brush foreBrush = new SolidBrush(textColor);
                    gr.DrawString(str, font, foreBrush, rc.X + 1, rc.Y + 1);
                    foreBrush.Dispose();
                }
            }
            if (fRegion) {
                ctl = (Control)host.RootComponent;
                rc = GetConvertedBounds(ctl);
                region.Intersect(rc);
                Region = region;
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.GetControlAtPoint"]/*' />
        /// <devdoc>
        ///     returns a control in the given tab vector that is at the given point, in
        ///     screen coords.
        /// </devdoc>
        private Control GetControlAtPoint(IList tabs, int x, int y) {
            IEnumerator e = tabs.GetEnumerator();
            Rectangle   rc;
            Control      ctlFound = null;
            Control      ctl;
            Control      parent;

            while (e.MoveNext()) {
                ctl = (Control)e.Current;
                parent = GetSitedParent(ctl);
                rc = ctl.Bounds;
                rc = parent.RectangleToScreen(rc);

                // We do not break if we find it here.  The vector is already setup
                // to have all controls in the current tabbing order, and child controls
                // are always after their parents.  If we broke, we wouldn't necessarially
                // find the appropriate child.
                //
                if (rc.Contains(x, y)) {
                    ctlFound = ctl;
                }
            }
            return ctlFound;
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.GetConvertedBounds"]/*' />
        /// <devdoc>
        ///     returns a rectangle in our own client space that represents the bounds
        ///     if the given control
        /// </devdoc>
        private Rectangle GetConvertedBounds(Control ctl) {
            Control parent = ctl.Parent;
            Rectangle rc = ctl.Bounds;
            rc = parent.RectangleToScreen(rc);
            rc = RectangleToClient(rc);
            return rc;
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.GetMaxControlCount"]/*' />
        /// <devdoc>
        ///     returns the maximum valid control count for the given control.  This
        ///     may be less than Control.getControlCount() because of invisible controls
        ///     and our own control
        /// </devdoc>
        private int GetMaxControlCount(Control ctl) {
            int count = 0;

            for (int n = 0; n < ctl.Controls.Count; n++) {
                if (GetTabbable(ctl.Controls[n])) {
                    count++;
                }
            }
            return count;
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.GetSitedParent"]/*' />
        /// <devdoc>
        ///     Retrieves the next parent control that would be usable
        ///     by the tab order UI.  We only want parents that are
        ///     sited by the designer host.
        /// </devdoc>
        private Control GetSitedParent(Control child) {
            Control parent = child.Parent;

            while (parent != null) {
                ISite site = parent.Site;
                if (site != null && site.Container == host) {
                    break;
                }
                parent = parent.Parent;
            }

            return parent;
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.GetTabbing"]/*' />
        /// <devdoc>
        ///     recursively fills the given tab vector with a control list
        /// </devdoc>
        private void GetTabbing(Control ctl, IList tabs) {
            Control ctlTab;
            
            // Now actually count the controls.  We add them to the list in reverse
            // order because the Controls collection is in z-order, and our list
            // needs to be in reverse z-order.  When done, we want this list to be
            // in z-order from back-most to top-most, and from parent-most to 
            // child-most.
            //
            int cnt = ctl.Controls.Count;
            for (int i = cnt - 1; i >= 0; i--) {
                ctlTab = ctl.Controls[i];

                if (null != GetSitedParent(ctlTab) && GetTabbable(ctlTab)) {
                    tabs.Add(ctlTab);
                }

                if (ctlTab.Controls.Count > 0) {
                    GetTabbing(ctlTab, tabs);
                }
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.GetTabbable"]/*' />
        /// <devdoc>
        ///     returns true if this component should show up in our tab list
        /// </devdoc>
        private bool GetTabbable(Control control) {

            for (Control c = control; c != null; c = c.Parent) {
                if (!c.Visible)
                    return false;
            }

            ISite site = control.Site;
            if (site == null || site.Container != host) {
                return false;
            }

            PropertyDescriptor prop = TypeDescriptor.GetProperties(control)["TabIndex"];

            if (prop == null || !prop.IsBrowsable) {
                return false;
            }

            tabProperties[control] = prop;
            return true;
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnComponentAddRemove"]/*' />
        /// <devdoc>
        ///     Called in response to a component add or remove event.  Here we re-aquire our
        ///     set of tabs.
        /// </devdoc>
        private void OnComponentAddRemove(object sender, ComponentEventArgs ce) {
            ctlHover = null;
            tabControls = null;
            tabGlyphs = null;
            
            if (tabComplete != null) {
                tabComplete.Clear();
            }
            if (tabNext != null) {
                tabNext.Clear();
            }
            if (region != null) {
                region.Dispose();
                region = null;
            }
            Invalidate();
        }
        
        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnComponentChanged"]/*' />
        /// <devdoc>
        ///      Called in response to a component change event.  Here we update our
        ///      tab order and redraw.
        /// </devdoc>
        private void OnComponentChanged(object sender, ComponentChangedEventArgs ce) {
            tabControls = null;
            tabGlyphs = null;
            if (region != null) {
                region.Dispose();
                region = null;
            }
            Invalidate();
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnKeyCancel"]/*' />
        /// <devdoc>
        ///      Closes the tab order UI.
        /// </devdoc>
        private void OnKeyCancel(object sender, EventArgs e) {
            IMenuCommandService mcs = (IMenuCommandService)host.GetService(typeof(IMenuCommandService));
            Debug.Assert(mcs != null, "No menu command service, can't get out of tab order UI");
            if (mcs != null) {
                MenuCommand mc = mcs.FindCommand(StandardCommands.TabOrder);
                Debug.Assert(mc != null, "No tab order menu command, can't get out of tab order UI");
                if (mc != null) {
                    mc.Invoke();
                }
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnKeyDefault"]/*' />
        /// <devdoc>
        ///      Sets the current tab order selection.
        /// </devdoc>
        private void OnKeyDefault(object sender, EventArgs e) {
            if (ctlHover != null) {
                SetNextTabIndex(ctlHover);
                RotateControls(true);
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnKeyNext"]/*' />
        /// <devdoc>
        ///      Selects the next component in the tab order.
        /// </devdoc>
        private void OnKeyNext(object sender, EventArgs e) {
            RotateControls(true);
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnKeyPrevious"]/*' />
        /// <devdoc>
        ///      Selects the previous component in the tab order.
        /// </devdoc>
        private void OnKeyPrevious(object sender, EventArgs e) {
            RotateControls(false);
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnMouseDoubleClick"]/*' />
        /// <devdoc>
        ///     This is called when the user double clicks on a component.  The typical
        ///     behavior is to create an event handler for the component's default event
        ///     and nativagate to the handler.
        /// </devdoc>
        public virtual void OnMouseDoubleClick(IComponent component) {
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnMouseDown"]/*' />
        /// <devdoc>
        ///     This is called when a mouse button is depressed.  This will perform
        ///     the default drag action for the selected components,  which is to
        ///     move those components around by the mouse.
        /// </devdoc>
        public virtual void OnMouseDown(IComponent component, MouseButtons button, int x, int y) {
            if (ctlHover != null) {
                SetNextTabIndex(ctlHover);
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnMouseDown1"]/*' />
        /// <devdoc>
        ///     Overrides control.OnMouseDown.  Here we set the tab index.  We must
        ///     do this as well as the above OnMouseDown to take into account clicks
        ///     in the tab index numbers.
        /// </devdoc>
        protected override void OnMouseDown(MouseEventArgs e) {
            base.OnMouseDown(e);
            if (ctlHover != null) {
                SetNextTabIndex(ctlHover);
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnMouseHover"]/*' />
        /// <devdoc>
        ///     This is called when the mouse momentarially hovers over the
        ///     view for the given component.
        /// </devdoc>
        public virtual void OnMouseHover(IComponent component) {
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnMouseMove"]/*' />
        /// <devdoc>
        ///     This is called for each movement of the mouse.
        /// </devdoc>
        public virtual void OnMouseMove(IComponent component, int x, int y) {
            if (tabControls != null) {
                Control ctl = GetControlAtPoint(tabControls, x, y);
                SetNewHover(ctl);
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnMouseMove1"]/*' />
        /// <devdoc>
        ///     Overrides control.  We update our cursor here.  We must do this
        ///     as well as the OnSetCursor to take into account mouse movements
        ///     over the tab index numbers.
        /// </devdoc>
        protected override void OnMouseMove(MouseEventArgs e) {
            base.OnMouseMove(e);
            
            if (tabGlyphs != null) {
            
                Control ctl = null;
                
                for(int i = 0; i < tabGlyphs.Length; i++) {
                    if (tabGlyphs[i].Contains(e.X, e.Y)) {
                        // Do not break if we find it -- we must
                        // work for nested children too.
                        ctl = (Control)tabControls[i];
                    }
                }
                
                SetNewHover(ctl);
            }

            if (ctlHover != null) {
                Cursor.Current = Cursors.Cross;
            }
            else {
                Cursor.Current = Cursors.Default;
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnMouseUp"]/*' />
        /// <devdoc>
        ///     This is called when the user releases the mouse from a component.
        ///     This will update the UI to reflect the release of the mouse.
        /// </devdoc>
        public virtual void OnMouseUp(IComponent component, MouseButtons button) {
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnSetCursor"]/*' />
        /// <devdoc>
        ///     This is called when the cursor for the given component should be updated.
        ///     The mouse is always over the given component's view when this is called.
        /// </devdoc>
        public virtual void OnSetCursor(IComponent component) {
            Cursor.Current = Cursors.Cross;
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OnPaint"]/*' />
        /// <devdoc>
        ///     Paints the tab control.
        /// </devdoc>
        protected override void OnPaint(PaintEventArgs e) {
            base.OnPaint(e);

            if (null == tabControls) {

                tabControls = new ArrayList();
                GetTabbing((Control)host.RootComponent, tabControls);
                tabGlyphs = new Rectangle[tabControls.Count];
            }
            if (null == tabComplete) {
                tabComplete = new ArrayList();
            }
            if (null == tabNext) {
                tabNext = new Hashtable();
            }
            if (null == region) {
                DrawTabs(tabControls, e.Graphics, true);
            }
            DrawTabs(tabControls, e.Graphics, false);
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OverrideInvoke"]/*' />
        /// <devdoc>
        ///     CommandSet will check with this handler on each status update
        ///     to see if the handler wants to override the availability of
        ///     this command.
        /// </devdoc>
        public bool OverrideInvoke(MenuCommand cmd) {
            for (int i = 0; i < commands.Length; i++) {
                if (commands[i].CommandID.Equals(cmd.CommandID)) {
                    commands[i].Invoke();
                    return true;
                }
            }

            return false;
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.OverrideStatus"]/*' />
        /// <devdoc>
        ///     CommandSet will check with this handler on each status update
        ///     to see if the handler wants to override the availability of
        ///     this command.
        /// </devdoc>
        public bool OverrideStatus(MenuCommand cmd) {

            for (int i = 0; i < commands.Length; i++) {
                if (commands[i].CommandID.Equals(cmd.CommandID)) {
                    cmd.Enabled = commands[i].Enabled;
                    return true;
                }
            }

            // Overriding the status of commands is easy.  We only
            // get commands that the designer implements, so we don't
            // have to pick and choose which ones to get rid of.  We
            // keep a select view and disable the rest.
            //
            if (!cmd.CommandID.Equals(StandardCommands.TabOrder)) {
                cmd.Enabled = false;
                return true;
            }

            return false;
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.RotateControls"]/*' />
        /// <devdoc>
        ///     Called when the keyboard has been pressed to rotate us
        ///     through the control list.
        /// </devdoc>
        private void RotateControls(bool forward) {
            Control ctl = ctlHover;
            Control form = (Control)host.RootComponent;

            if (ctl == null) {
                ctl = form;
            }

            while (null != (ctl = form.GetNextControl(ctl, forward))) {
                if (GetTabbable(ctl))
                    break;
            }
            SetNewHover(ctl);
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.SetNewHover"]/*' />
        /// <devdoc>
        ///     Establishes a new hover control.
        /// </devdoc>
        private void SetNewHover(Control ctl) {

            if (ctlHover != ctl) {
                if (null != ctlHover) {
                    if (region != null) {
                        region.Dispose();
                        region = null;
                    }
                    Rectangle rc = GetConvertedBounds(ctlHover);
                    rc.Inflate(selSize, selSize);
                    Invalidate(rc);
                }

                ctlHover = ctl;

                if (null != ctlHover) {
                    if (region != null) {
                        region.Dispose();
                        region = null;
                    }
                    Rectangle rc = GetConvertedBounds(ctlHover);
                    rc.Inflate(selSize, selSize);
                    Invalidate(rc);
                }
            }
        }

        /// <include file='doc\TabOrder.uex' path='docs/doc[@for="TabOrder.SetNextTabIndex"]/*' />
        /// <devdoc>
        ///     sets up the next tab index for the given control
        /// </devdoc>
        private void SetNextTabIndex(Control ctl) {
            int index, max;
            Control parent = GetSitedParent(ctl);
            object nextIndex = tabNext[parent];

            if (tabComplete.IndexOf(ctl)==-1)
                tabComplete.Add(ctl);

            if (null != nextIndex)
                index = (int)nextIndex;
            else
                index = 0;

            try {
                PropertyDescriptor prop = (PropertyDescriptor)tabProperties[ctl];
                if (prop != null) {
                
                    int newIndex = index + 1;

                    if (prop.IsReadOnly) {
                        newIndex = (int)prop.GetValue(ctl) + 1;
                    }

                    max = GetMaxControlCount(parent);

                    if (newIndex >= max) {
                        newIndex = 0;
                    }

                    tabNext[parent] = newIndex;

                    if (tabComplete.Count == tabControls.Count)
                        tabComplete.Clear();
                        
                    // Now set the property
                    //
                    if (!prop.IsReadOnly) {
                        try {
                            prop.SetValue(ctl, index);
                        }
                        catch(Exception) {
                        }
                    }
                    else {
                        // If the property is read only, we still count it
                        // so that other properties can "flow" around it.
                        // Therefore, we need a paint.
                        //
                        Invalidate();
                    }
                }
            }
            catch (Exception) {
            }
        }
    }
}

