/*
*  Copyright (C) 2000 Brian Harris
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef BOOKMARK_NODE_H
#define BOOKMARK_NODE_H

#include <string>
#include <vector>

const BOOKMARK_BOOKMARK = 0;  // this node is a real bookmark, all fields valid
const BOOKMARK_FOLDER   = 1;  // "children" is not empty.  url is not used
const BOOKMARK_FOLDER_TB= 2;  // Toolbar Folder, "children" is not empty.  url is not used
const BOOKMARK_SEPARATOR= 3;  // this node is a separator, url, title, and children are not used

class CBookmarkNode {
public:
	int id;
   std::string text;
	std::string url;
	int type;
   time_t addDate;
   time_t lastVisit;
   time_t lastModified;
   CBookmarkNode *next;
   CBookmarkNode *child;
   CBookmarkNode *lastChild;

   inline CBookmarkNode()
   {
      id = 0;
      text = "";
      url = "";
      type = 0;
      next = NULL;
      child = NULL;
      lastChild = NULL;

      addDate = 0;
      lastVisit = 0;
      lastModified = 0;
   }
	inline CBookmarkNode(int id, const char *text, const char *url, int type, time_t addDate=0, time_t lastVisit=0, time_t lastModified=0)
   {
      this->id = id;
      this->text = text;
      this->url = url;
      this->type = type;
      this->next = NULL;
      this->child = NULL;
      this->lastChild = NULL;
      this->addDate = addDate;
      this->lastVisit = lastVisit;
      this->lastModified = lastModified;
   }
   inline CBookmarkNode(int id, std::string &text, std::string &url, int type, time_t addDate=0, time_t lastVisit=0, time_t lastModified=0)
   {
      this->id = id;
      this->text = text;
      this->url = url;
      this->type = type;
      this->next = NULL;
      this->child = NULL;
      this->lastChild = NULL;
      this->addDate = addDate;
      this->lastVisit = lastVisit;
      this->lastModified = lastModified;
   }
   inline ~CBookmarkNode()
   {
      // when we delete next, it's destructor will delete it's next, which will delete it's next, etc...
      if (next)
         delete next;

      // same with child...
      if (child)
         delete child;
   }
   inline CBookmarkNode &operator=(CBookmarkNode &n2)
   {
      id = n2.id;
      text = n2.text;
      url = n2.url;
      type = n2.type;
      addDate = n2.addDate;
      lastVisit = n2.lastVisit;
      lastModified = n2.lastModified;

      if (child) {
         delete child;
      }
      if (next) {
         delete next;
      }

      if (n2.child) {
         child = new CBookmarkNode();
         (*child) = (*n2.child);
      }

      if (n2.next) {
         next = new CBookmarkNode();
         (*next) = (*n2.next);
      }

      return *this;
   }
   inline AddChild(CBookmarkNode *newChild)
   {
      if (child) {
         lastChild->next = newChild;
      }
      else {
         child = newChild;
      }
      lastChild = newChild;
   }
   BOOL DeleteNode(CBookmarkNode *node)
   {
      CBookmarkNode *c;
      CBookmarkNode *previous = NULL;

      for (c=child; c; previous=c, c=c->next) {
         if (c == node) {
            // found our node to delete!

            // first redirect traffic around it
            if (previous) {
               previous->next = node->next;
            }
            else {
               child = node->next;
            }

            // if we are the last item, set our parent's lastChild to the item before us
            if (!node->next) {
               lastChild = previous;
            }

            // WE HAVE TO SET OUR NEXT TO NULL
            // if we don't do this, when we are deleted, we will try to delete our next, which would result
            // in pretty much the whole menu being deleted
            node->next = NULL;

            // finally we can be deleted
            delete node;

            // WE HAVE TO BREAK HERE
            // if we don't break, when the for () tries to do child=child->next, it will crash
            return true;
         }
      }
      return false;
   }
   CBookmarkNode *FindNode(int id)
   {
      CBookmarkNode *c;
      for (c=child; c; c=c->next) {
         if (c->type == BOOKMARK_SEPARATOR) {
            continue;
         }
         else if (c->type == BOOKMARK_FOLDER || c->type == BOOKMARK_FOLDER_TB) {
            CBookmarkNode *lc = c->lastChild;

            // this little bit of code is for optimizations
            // it assumes the items are stored in order.
            // if the items ever get out of order, we need to remove this code

            if (lc && lc->type == BOOKMARK_BOOKMARK && lc->id < id) {
               continue;
            }

            CBookmarkNode *retNode = c->FindNode(id);

            if (retNode != c) {
               // found it in a sub-node
               return retNode;
            }
         }
         else if (c->id == id) {
            // this is it!
            return c;
         }
      }
      // We couldn't find it.  Rather than returning null and risking a null pointer crash, return ourself
      return this;
   }
   CBookmarkNode *FindToolbarNode()
   {
      CBookmarkNode *c;
      for (c=child; c; c=c->next) {
         if (c->type == BOOKMARK_FOLDER_TB) {
            // this is it!
            return c;
         }
         else if (c->type == BOOKMARK_FOLDER) {
            CBookmarkNode *lc = c->lastChild;

            CBookmarkNode *retNode = c->FindToolbarNode();

            if (retNode != c) {
               // found it in a sub-node
               return retNode;
            }
         }
      }
      // We couldn't find it.  Rather than returning null and risking a null pointer crash, return ourself
      return this;
   }
};

#endif