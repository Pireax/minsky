/*
  @copyright Steve Keen 2015
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "item.h"
#include "group.h"
#include "zoom.h"
#include "variable.h"
#include "latexMarkup.h"
#include "geometry.h"
#include "selection.h"
#include <pango.h>
#include <cairo_base.h>
#include <ecolab_epilogue.h>
#include <exception>

using ecolab::Pango;
using namespace std;

namespace minsky
{

  void BoundingBox::update(const Item& x)
  {
    ecolab::cairo::Surface surf
       (cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL));
    auto savedMouseFocus=x.mouseFocus;
    x.mouseFocus=false; // do not mark up icon with tooltips etc, which might invalidate this calc
    try {x.draw(surf.cairo());}
    catch (const std::exception& e) 
      {cerr<<"illegal exception caught in draw()"<<e.what()<<endl;}
    catch (...) {cerr<<"illegal exception caught in draw()";}
    x.mouseFocus=savedMouseFocus;
    double l,t,w,h;
    cairo_recording_surface_ink_extents(surf.surface(),
                                        &l,&t,&w,&h);
    // note (0,0) is relative to the (x,y) of icon.
    double invZ=1/x.zoomFactor;
    left=l*invZ;
    right=(l+w)*invZ;
    top=t*invZ;
    bottom=(t+h)*invZ;
  }

  float Item::x() const 
  {
    if (auto g=group.lock())
      return g->zoomFactor*m_x+g->x();
    else
      return m_x;
  }

  float Item::y() const 
  {
    if (auto g=group.lock())
      return g->zoomFactor*m_y+g->y();
    else
      return m_y;
  }

  void Item::deleteAttachedWires()
  {
    for (auto& p: ports)
      p->deleteWires();
  }
  
  bool Item::visible() const 
  {
    if (auto g=group.lock())
      return m_visible && g->displayContents();
    else
      return m_visible;
  }
  

  void Item::moveTo(float x, float y)
  {
    if (auto g=group.lock())
      {
        m_x=(x-g->x())/g->zoomFactor;
        m_y=(y-g->y())/g->zoomFactor;
      }
    else
      {
        m_x=x;
        m_y=y;
      }
    assert(abs(x-this->x())<1 && abs(y-this->y())<1);
  }

  ClickType::Type Item::clickType(float x, float y)
  {
    // if selecting a contained variable, the delegate to that
    if (auto item=select(x,y))
      return item->clickType(x,y);
    
    // firstly, check whether a port has been selected
    for (auto& p: ports)
      {
        if (hypot(x-p->x(), y-p->y()) < portRadius*zoomFactor)
          return ClickType::onPort;
      }
    ecolab::cairo::Surface dummySurf
      (cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,nullptr));
    draw(dummySurf.cairo());
    if (cairo_in_clip(dummySurf.cairo(), (x-this->x()), (y-this->y())))
      return ClickType::onItem;
    else
      return ClickType::outside;
  }

  void Item::zoom(float xOrigin, float yOrigin,float factor)
  {
    if (visible())
      zoomFactor*=factor;
  }

  void Item::drawPorts(cairo_t* cairo) const
  {
    cairo_save(cairo);
    cairo_new_path(cairo);
    for (auto& p: ports)
      {
        cairo_new_sub_path(cairo);
        cairo_arc(cairo, p->x()-x(), p->y()-y(), portRadius*zoomFactor, 0, 2*M_PI);
      }
    cairo_set_source_rgb(cairo, 0,0,0);
    cairo_set_line_width(cairo,1);
    cairo_stroke(cairo);
    cairo_restore(cairo);
  }

  void Item::drawSelected(cairo_t* cairo) const
  {
    // implemented by filling the clip region with a transparent grey
    cairo_save(cairo);
    cairo_set_source_rgba(cairo, 0.5,0.5,0.5,0.4);
    cairo_paint(cairo);
    cairo_restore(cairo);
  }

  // default is just to display the detailed text (ie a "note")
  void Item::draw(cairo_t* cairo) const
  {
    Rotate r(rotation,0,0);
    Pango pango(cairo);
    pango.angle=rotation * M_PI / 180.0;
    pango.setFontSize(12*zoomFactor);
    pango.setMarkup(latexToPango(detailedText)); 
    // parameters of icon in userspace (unscaled) coordinates
    float w, h;
    w=0.5*pango.width()+2*zoomFactor; 
    h=0.5*pango.height()+4*zoomFactor;

    cairo_move_to(cairo,r.x(-w+1,-h+2), r.y(-w+1,-h+2));
    pango.show();

    if (mouseFocus) displayTooltip(cairo);
    cairo_move_to(cairo,r.x(-w,-h), r.y(-w,-h));
    cairo_line_to(cairo,r.x(w,-h), r.y(w,-h));
    cairo_line_to(cairo,r.x(w,h), r.y(w,h));
    cairo_line_to(cairo,r.x(-w,h), r.y(-w,h));
    cairo_close_path(cairo);
    cairo_clip(cairo);
    if (selected) drawSelected(cairo);
  }

  void Item::dummyDraw() const
  {
    ecolab::cairo::Surface s(cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL));
    draw(s.cairo());
  }

  void Item::displayTooltip(cairo_t* cairo) const
  {
    if (!tooltip.empty())
      {
        cairo_save(cairo);
        Pango pango(cairo);
        pango.setMarkup(latexToPango(tooltip));
        cairo_translate(cairo,zoomFactor*(0.5*bb.width())+10,
                        zoomFactor*(-0.5*bb.height())-20);
        cairo_rectangle(cairo,0,0,pango.width(),pango.height());
        cairo_set_source_rgb(cairo,1,1,1);
        cairo_fill_preserve(cairo);
        cairo_set_source_rgb(cairo,0,0,0);
        pango.show();
        cairo_stroke(cairo);
        cairo_restore(cairo);
      }
  }

  shared_ptr<Port> Item::closestOutPort(float x, float y) const 
  {
    if (auto v=select(x,y))
      return v->closestOutPort(x,y);
    return ports.size()>0 && !ports[0]->input()? ports[0]: nullptr;
  }
  
  shared_ptr<Port> Item::closestInPort(float x, float y) const
  {
    if (auto v=select(x,y))
      return v->closestInPort(x,y);
    shared_ptr<Port> r;
    for (size_t i=0; i<ports.size(); ++i)
      if (ports[i]->input() &&
          (!r || sqr(ports[i]->x()-x)+sqr(ports[i]->y()-y) <
           sqr(r->x()-x)+sqr(r->y()-y)))
        r=ports[i];
    return r;
  }

  ItemPtr Item::select(float x, float y) const
  {return ItemPtr();}


}
