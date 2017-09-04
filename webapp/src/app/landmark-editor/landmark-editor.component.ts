import { Component, OnInit, ElementRef, ViewChild, HostListener } from '@angular/core';
import { Input, Output } from '@angular/core'

import * as $ from 'jquery'
import * as interact from 'interactjs';
import { Point } from '../model/datatypes'

@Component({
  selector: 'app-landmark-editor',
  templateUrl: './landmark-editor.component.html',
  styleUrls: ['./landmark-editor.component.css']
})
export class LandmarkEditorComponent implements OnInit {

  private _inputPhoto: string = "data:image/gif;base64,R0lGODlhAQABAAAAACwAAAAAAQABAAA=";

  public crownPoint: Point;
  public chinPoint: Point;

  private _imageWidth: number;
  private _imageHeight: number;
  private _viewPortWidth: number;
  private _viewPortHeight: number;

  private _xleft: number; // Offset in screen pixels
  private _ytop: number;
  private _zoom: number;
  private _ratio: number;   // Ratio between image pixels and screen pixels

  landMarksVisible: boolean = false;

  private _imgElmt: any;
  private _containerElmt: any;
  private _crownMarkElmt: any;
  private _chinMarkElmt: any;

  @Input()
  set inputPhoto(value: string) {
    var newImg = new Image();
    let that = this;
    newImg.onload = function () {
      that._imageWidth = newImg.width;
      that._imageHeight = newImg.height;
      that._inputPhoto = value;
      if (that._imageWidth > 100 && that._imageHeight > 100) {
        that.calculateViewPort();
        that.zoomFit();
        that.renderImage();
        that.renderLandMarks();
      }
    };
    newImg.src = value;
  }
  get inputPhoto(): string {
    return this._inputPhoto;
  }

  constructor(private el: ElementRef) {
    this.crownPoint = new Point(0, 0);
    this.chinPoint = new Point(0, 0);
  }

  ngOnInit() {
    this._imgElmt = this.el.nativeElement.querySelector('#photo');
    this._containerElmt = this.el.nativeElement.querySelector('#container');
    this._crownMarkElmt = this.el.nativeElement.querySelector('#crownMark');
    this._chinMarkElmt = this.el.nativeElement.querySelector('#chinMark');

    let that = this;
    interact('.landmark')
      .draggable({
        // enable inertial throwing
        inertia: false,
        // keep the element within the area of it's parent
        restrict: {
          restriction: "parent",
          endOnly: true,
          elementRect: { top: 0, left: 0, bottom: 1, right: 1 }
        },
        // call this function on every dragmove event
        onmove: function (event) {
          let target = event.target;
          // keep the dragged position in the x/y attributes
          let x = (parseFloat(target.getAttribute('x')) || 0) + event.dx;
          let y = (parseFloat(target.getAttribute('y')) || 0) + event.dy;
          // translate the element
          that.translateElement(target, new Point(x, y));
        },
        // call this function on every dragend event
        onend: function (event) {
          //that.updateLandMarks();
        }
      });
    this.el.nativeElement.addEventListener
  }
  zoomFit(): void {
    let xratio = this._viewPortWidth / this._imageWidth;
    let yratio = this._viewPortHeight / this._imageHeight;
    this._ratio = xratio < yratio ? xratio : yratio;
    this._xleft = this._viewPortWidth / 2 - this._ratio * this._imageWidth / 2;
    this._ytop = this._viewPortHeight / 2 - this._ratio * this._imageHeight / 2;
  };
  calculateViewPort(): void {
    this._viewPortWidth = this._containerElmt.clientWidth;
    this._viewPortHeight = this._containerElmt.clientHeight;
  };

  @HostListener('window:resize', ['$event'])

  onResize(event) {

    this.calculateViewPort();
    this.zoomFit()
    this.renderImage();
    this.renderLandMarks();
  }

  renderImage(): void {
    let xw = this._imageWidth * this._ratio;
    let yh = this._imageHeight * this._ratio;
    this._imgElmt.width = xw;
    this._imgElmt.height = yh;
    this.translateElement(this._imgElmt, new Point(this._xleft, this._ytop));
  }

  translateElement(elmt: any, pt: Point) {
    // Translate the element position
    elmt.style.transform = elmt.style.webkitTransform = `translate(${pt.x}px, ${pt.y}px)`;
    // Store it in attached properties
    elmt.setAttribute('x', pt.x);
    elmt.setAttribute('y', pt.y);
    console.log(elmt)
  };

  setLandMarks(crownPoint: Point, chinPoint: Point): void {
    this.crownPoint = crownPoint;
    this.chinPoint = chinPoint;
    this.renderLandMarks();
  }

  renderLandMarks() : void {
    // Testing data
    //this.crownPoint = new Point(1.136017e+003, 6.216124e+002);
    //this.chinPoint = new Point(1.136017e+003, 1.701095e+003);

    if (this.crownPoint && this.crownPoint.x && this.crownPoint.y
        && this.chinPoint && this.chinPoint.x && this.chinPoint.y
        && this._imageWidth > 100 && this._imageHeight > 100) {
      let p1 = this.pixelToScreen(this._crownMarkElmt, this.crownPoint);
      let p2 = this.pixelToScreen(this._chinMarkElmt, this.chinPoint);
      this.translateElement(this._crownMarkElmt, p1);
      this.translateElement(this._chinMarkElmt, p2);
      $(".landmark").css('visibility', 'visible');
    } else {
      $(".landmark").css('visibility', 'hidden');
    }
  };

  pixelToScreen(elmt: any, pt: Point): Point {
    return new Point(
      this._xleft + pt.x * this._ratio - elmt.clientWidth / 2,
      this._ytop + pt.y * this._ratio - elmt.clientHeight / 2);
  };

  screenToPixel(elmt: any): Point {
    return new Point(
      (parseFloat(elmt.getAttribute('x')) + elmt.clientWidth / 2 - this._xleft) / this._ratio,
      (parseFloat(elmt.getAttribute('y')) + elmt.clientHeight / 2 - this._ytop) / this._ratio);
  }

  updateLandMarks() {
    this.crownPoint = this.screenToPixel(this._crownMarkElmt);
    this.chinPoint = this.screenToPixel(this._chinMarkElmt);
  };
}