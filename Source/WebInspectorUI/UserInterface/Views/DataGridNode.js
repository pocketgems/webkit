/*
 * Copyright (C) 2008, 2013-2016 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DataGridNode = class DataGridNode extends WebInspector.Object
{
    constructor(data, hasChildren)
    {
        super();

        this._expanded = false;
        this._hidden = false;
        this._selected = false;
        this._copyable = true;
        this._shouldRefreshChildren = true;
        this._data = data || {};
        this.hasChildren = hasChildren || false;
        this.children = [];
        this.dataGrid = null;
        this.parent = null;
        this.previousSibling = null;
        this.nextSibling = null;
        this.disclosureToggleWidth = 10;
    }

    get hidden()
    {
        return this._hidden;
    }

    set hidden(x)
    {
        x = !!x;

        if (this._hidden === x)
            return;

        this._hidden = x;
        if (this._element)
            this._element.classList.toggle("hidden", this._hidden);

        if (this.dataGrid)
            this.dataGrid._noteRowsChanged();
    }

    get selectable()
    {
        return this._element && !this._hidden;
    }

    get copyable()
    {
        return this._copyable;
    }

    set copyable(x)
    {
        this._copyable = x;
    }

    get element()
    {
        if (this._element)
            return this._element;

        if (!this.dataGrid)
            return null;

        this._element = document.createElement("tr");
        this._element._dataGridNode = this;

        if (this.hasChildren)
            this._element.classList.add("parent");
        if (this.expanded)
            this._element.classList.add("expanded");
        if (this.selected)
            this._element.classList.add("selected");
        if (this.revealed)
            this._element.classList.add("revealed");
        if (this._hidden)
            this._element.classList.add("hidden");

        this.createCells();
        return this._element;
    }

    createCells()
    {
        for (var columnIdentifier of this.dataGrid.orderedColumns)
            this._element.appendChild(this.createCell(columnIdentifier));
    }

    refreshIfNeeded()
    {
        if (!this._needsRefresh)
            return;

        this._needsRefresh = false;

        this.refresh();
    }

    needsRefresh()
    {
        this._needsRefresh = true;

        if (!this._revealed)
            return;

        if (this._scheduledRefreshIdentifier)
            return;

        this._scheduledRefreshIdentifier = requestAnimationFrame(this.refresh.bind(this));
    }

    get data()
    {
        return this._data;
    }

    set data(x)
    {
        console.assert(typeof x === "object", "Data should be an object.");

        x = x || {};

        if (Object.shallowEqual(this._data, x))
            return;

        this._data = x;
        this.needsRefresh();
    }

    get filterableData()
    {
        if (this._cachedFilterableData)
            return this._cachedFilterableData;

        this._cachedFilterableData = [];

        for (let column of this.dataGrid.columns.values()) {
            if (column.hidden)
                continue;

            let value = this.filterableDataForColumn(column.columnIdentifier);
            if (!value)
                continue;

            if (!(value instanceof Array))
                value = [value];

            if (!value.length)
                continue;

            this._cachedFilterableData = this._cachedFilterableData.concat(value);
        }

        return this._cachedFilterableData;
    }

    get revealed()
    {
        if ("_revealed" in this)
            return this._revealed;

        var currentAncestor = this.parent;
        while (currentAncestor && !currentAncestor.root) {
            if (!currentAncestor.expanded) {
                this._revealed = false;
                return false;
            }

            currentAncestor = currentAncestor.parent;
        }

        this._revealed = true;
        return true;
    }

    set hasChildren(x)
    {
        if (this._hasChildren === x)
            return;

        this._hasChildren = x;

        if (!this._element)
            return;

        if (this._hasChildren) {
            this._element.classList.add("parent");
            if (this.expanded)
                this._element.classList.add("expanded");
        } else
            this._element.classList.remove("parent", "expanded");
    }

    get hasChildren()
    {
        return this._hasChildren;
    }

    set revealed(x)
    {
        if (this._revealed === x)
            return;

        this._revealed = x;

        if (this._element) {
            if (this._revealed)
                this._element.classList.add("revealed");
            else
                this._element.classList.remove("revealed");
        }

        this.refreshIfNeeded();

        for (var i = 0; i < this.children.length; ++i)
            this.children[i].revealed = x && this.expanded;
    }

    get depth()
    {
        if ("_depth" in this)
            return this._depth;
        if (this.parent && !this.parent.root)
            this._depth = this.parent.depth + 1;
        else
            this._depth = 0;
        return this._depth;
    }

    get leftPadding()
    {
        if (typeof this._leftPadding === "number")
            return this._leftPadding;

        this._leftPadding = this.depth * this.dataGrid.indentWidth;
        return this._leftPadding;
    }

    get shouldRefreshChildren()
    {
        return this._shouldRefreshChildren;
    }

    set shouldRefreshChildren(x)
    {
        this._shouldRefreshChildren = x;
        if (x && this.expanded)
            this.expand();
    }

    get selected()
    {
        return this._selected;
    }

    set selected(x)
    {
        if (x)
            this.select();
        else
            this.deselect();
    }

    get expanded()
    {
        return this._expanded;
    }

    set expanded(x)
    {
        if (x)
            this.expand();
        else
            this.collapse();
    }

    hasAncestor(ancestor)
    {
        if (!ancestor)
            return false;

        let currentAncestor = this.parent;
        while (currentAncestor) {
            if (ancestor === currentAncestor)
                return true;

            currentAncestor = currentAncestor.parent;
        }

        return false;
    }

    refresh()
    {
        if (!this._element || !this.dataGrid)
            return;

        if (this._scheduledRefreshIdentifier) {
            cancelAnimationFrame(this._scheduledRefreshIdentifier);
            this._scheduledRefreshIdentifier = undefined;
        }

        this._cachedFilterableData = null;
        this._needsRefresh = false;

        this._element.removeChildren();
        this.createCells();
    }

    refreshRecursively()
    {
        this.refresh();
        this.forEachChildInSubtree((node) => node.refresh());
    }

    updateLayout()
    {
        // Implemented by subclasses if needed.
    }

    createCell(columnIdentifier)
    {
        var cellElement = document.createElement("td");
        cellElement.className = columnIdentifier + "-column";
        cellElement.__columnIdentifier = columnIdentifier;

        var column = this.dataGrid.columns.get(columnIdentifier);

        if (column["aligned"])
            cellElement.classList.add(column["aligned"]);

        if (column["group"])
            cellElement.classList.add("column-group-" + column["group"]);

        var div = cellElement.createChild("div", "cell-content");
        var content = this.createCellContent(columnIdentifier, cellElement);
        div.append(content);

        if (column["icon"]) {
            let iconElement = document.createElement("div");
            iconElement.classList.add("icon");
            div.insertBefore(iconElement, div.firstChild);
        }

        if (columnIdentifier === this.dataGrid.disclosureColumnIdentifier) {
            cellElement.classList.add("disclosure");
            if (this.leftPadding)
                cellElement.style.setProperty("padding-left", this.leftPadding + "px");
        }

        return cellElement;
    }

    createCellContent(columnIdentifier)
    {
        return this.data[columnIdentifier] || "\u200b"; // Zero width space to keep the cell from collapsing.
    }

    elementWithColumnIdentifier(columnIdentifier)
    {
        if (!this.dataGrid)
            return null;

        let index = this.dataGrid.orderedColumns.indexOf(columnIdentifier);
        if (index === -1)
            return null;

        return this.element.children[index];
    }

    // Share these functions with DataGrid. They are written to work with a DataGridNode this object.
    appendChild() { return WebInspector.DataGrid.prototype.appendChild.apply(this, arguments); }
    insertChild() { return WebInspector.DataGrid.prototype.insertChild.apply(this, arguments); }
    removeChild() { return WebInspector.DataGrid.prototype.removeChild.apply(this, arguments); }
    removeChildren() { return WebInspector.DataGrid.prototype.removeChildren.apply(this, arguments); }
    removeChildrenRecursive() { return WebInspector.DataGrid.prototype.removeChildrenRecursive.apply(this, arguments); }

    _recalculateSiblings(myIndex)
    {
        if (!this.parent)
            return;

        var previousChild = myIndex > 0 ? this.parent.children[myIndex - 1] : null;

        if (previousChild) {
            previousChild.nextSibling = this;
            this.previousSibling = previousChild;
        } else
            this.previousSibling = null;

        var nextChild = this.parent.children[myIndex + 1];

        if (nextChild) {
            nextChild.previousSibling = this;
            this.nextSibling = nextChild;
        } else
            this.nextSibling = null;
    }

    collapse()
    {
        if (this._element)
            this._element.classList.remove("expanded");

        this._expanded = false;

        for (var i = 0; i < this.children.length; ++i)
            this.children[i].revealed = false;

        this.dispatchEventToListeners("collapsed");

        if (this.dataGrid) {
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.CollapsedNode, {dataGridNode: this});
            this.dataGrid._noteRowsChanged();
        }
    }

    collapseRecursively()
    {
        var item = this;
        while (item) {
            if (item.expanded)
                item.collapse();
            item = item.traverseNextNode(false, this, true);
        }
    }

    expand()
    {
        if (!this.hasChildren || this.expanded)
            return;

        if (this.revealed && !this._shouldRefreshChildren)
            for (var i = 0; i < this.children.length; ++i)
                this.children[i].revealed = true;

        if (this._shouldRefreshChildren) {
            for (var i = 0; i < this.children.length; ++i)
                this.children[i]._detach();

            this.dispatchEventToListeners("populate");

            if (this._attached) {
                for (var i = 0; i < this.children.length; ++i) {
                    var child = this.children[i];
                    if (this.revealed)
                        child.revealed = true;
                    child._attach();
                }
            }

            this._shouldRefreshChildren = false;
        }

        if (this._element)
            this._element.classList.add("expanded");

        this._expanded = true;

        this.dispatchEventToListeners("expanded");

        if (this.dataGrid) {
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.ExpandedNode, {dataGridNode: this});
            this.dataGrid._noteRowsChanged();
        }
    }

    expandRecursively()
    {
        var item = this;
        while (item) {
            item.expand();
            item = item.traverseNextNode(false, this);
        }
    }

    forEachImmediateChild(callback)
    {
        for (let node of this.children)
            callback(node);
    }

    forEachChildInSubtree(callback)
    {
        let node = this.traverseNextNode(false, this, true);
        while (node) {
            callback(node);
            node = node.traverseNextNode(false, this, true);
        }
    }

    isInSubtreeOfNode(baseNode)
    {
        let node = baseNode;
        while (node) {
            if (node === this)
                return true;
            node = node.traverseNextNode(false, baseNode, true);
        }
        return false;
    }

    reveal()
    {
        var currentAncestor = this.parent;
        while (currentAncestor && !currentAncestor.root) {
            if (!currentAncestor.expanded)
                currentAncestor.expand();
            currentAncestor = currentAncestor.parent;
        }

        this.element.scrollIntoViewIfNeeded(false);

        this.dispatchEventToListeners("revealed");
    }

    select(suppressSelectedEvent)
    {
        if (!this.dataGrid || !this.selectable || this.selected)
            return;

        let oldSelectedNode = this.dataGrid.selectedNode;
        if (oldSelectedNode)
            oldSelectedNode.deselect(true);

        this._selected = true;
        this.dataGrid.selectedNode = this;

        if (this._element)
            this._element.classList.add("selected");

        if (!suppressSelectedEvent)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.SelectedNodeChanged, {oldSelectedNode});
    }

    revealAndSelect()
    {
        this.reveal();
        this.select();
    }

    deselect(suppressDeselectedEvent)
    {
        if (!this.dataGrid || this.dataGrid.selectedNode !== this || !this.selected)
            return;

        this._selected = false;
        this.dataGrid.selectedNode = null;

        if (this._element)
            this._element.classList.remove("selected");

        if (!suppressDeselectedEvent)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.SelectedNodeChanged, {oldSelectedNode: this});
    }

    traverseNextNode(skipHidden, stayWithin, dontPopulate, info)
    {
        if (!dontPopulate && this.hasChildren)
            this.dispatchEventToListeners("populate");

        if (info)
            info.depthChange = 0;

        var node = (!skipHidden || this.revealed) ? this.children[0] : null;
        if (node && (!skipHidden || this.expanded)) {
            if (info)
                info.depthChange = 1;
            return node;
        }

        if (this === stayWithin)
            return null;

        node = (!skipHidden || this.revealed) ? this.nextSibling : null;
        if (node)
            return node;

        node = this;
        while (node && !node.root && !((!skipHidden || node.revealed) ? node.nextSibling : null) && node.parent !== stayWithin) {
            if (info)
                info.depthChange -= 1;
            node = node.parent;
        }

        if (!node)
            return null;

        return (!skipHidden || node.revealed) ? node.nextSibling : null;
    }

    traversePreviousNode(skipHidden, dontPopulate)
    {
        var node = (!skipHidden || this.revealed) ? this.previousSibling : null;
        if (!dontPopulate && node && node.hasChildren)
            node.dispatchEventToListeners("populate");

        while (node && ((!skipHidden || (node.revealed && node.expanded)) ? node.children.lastValue : null)) {
            if (!dontPopulate && node.hasChildren)
                node.dispatchEventToListeners("populate");
            node = ((!skipHidden || (node.revealed && node.expanded)) ? node.children.lastValue : null);
        }

        if (node)
            return node;

        if (!this.parent || this.parent.root)
            return null;

        return this.parent;
    }

    isEventWithinDisclosureTriangle(event)
    {
        if (!this.hasChildren)
            return false;
        let cell = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!cell || !cell.classList.contains("disclosure"))
            return false;

        let computedLeftPadding = window.getComputedStyle(cell).getPropertyCSSValue("padding-left").getFloatValue(CSSPrimitiveValue.CSS_PX);
        let left = cell.totalOffsetLeft + computedLeftPadding;
        return event.pageX >= left && event.pageX <= left + this.disclosureToggleWidth;
    }

    _attach()
    {
        if (!this.dataGrid || this._attached)
            return;

        this._attached = true;

        let insertionIndex = -1;

        if (!this.isPlaceholderNode) {
            var previousGridNode = this.traversePreviousNode(true, true);
            insertionIndex = this.dataGrid._rows.indexOf(previousGridNode);
            if (insertionIndex === -1)
                insertionIndex = 0;
            else
                insertionIndex++;
        }

        if (insertionIndex === -1)
            this.dataGrid._rows.push(this);
        else
            this.dataGrid._rows.insertAtIndex(this, insertionIndex);

        this.dataGrid._noteRowsChanged();

        if (this.expanded) {
            for (var i = 0; i < this.children.length; ++i)
                this.children[i]._attach();
        }
    }

    _detach()
    {
        if (!this._attached)
            return;

        this._attached = false;

        this.dataGrid._rows.remove(this, true);
        this.dataGrid._noteRowRemoved(this);

        for (var i = 0; i < this.children.length; ++i)
            this.children[i]._detach();
    }

    savePosition()
    {
        if (this._savedPosition)
            return;

        console.assert(this.parent);
        if (!this.parent)
            return;

        this._savedPosition = {
            parent: this.parent,
            index: this.parent.children.indexOf(this)
        };
    }

    restorePosition()
    {
        if (!this._savedPosition)
            return;

        if (this.parent !== this._savedPosition.parent)
            this._savedPosition.parent.insertChild(this, this._savedPosition.index);

        this._savedPosition = null;
    }

    appendContextMenuItems(contextMenu)
    {
        // Subclasses may override
        return null;
    }

    // Protected

    filterableDataForColumn(columnIdentifier)
    {
        let value = this.data[columnIdentifier];
        return typeof value === "string" ? value : null;
    }
};

// Used to create a new table row when entering new data by editing cells.
WebInspector.PlaceholderDataGridNode = class PlaceholderDataGridNode extends WebInspector.DataGridNode
{
    constructor(data)
    {
        super(data, false);
        this.isPlaceholderNode = true;
    }

    makeNormal()
    {
        this.isPlaceholderNode = false;
    }
};
