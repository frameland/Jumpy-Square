Strict
Import vsat

#rem
Class LabelScene Extends VScene
	Field feed:LabelFeed = New LabelFeed
	
	Method OnInit:Void()
		feed.InitWithSizeAndFont(4, "lane_narrow")
		feed.position.Add(Vsat.ScreenWidth2, Vsat.ScreenHeight2)
	End
	
	Method OnUpdate:Void(dt:Float)
		feed.Update(dt)
		If KeyHit(KEY_SPACE) 
			feed.Push("Double Dodge")
		End
		
	End

	Method OnRender:Void()
		feed.Render()
	End
	
End
#end


Interface ILabelFeedCallback
	Method OnLabelPush:Void(item:LabelFeedItem)
End


Class LabelFeed Extends VRect
	
	Field lineHeightMultiplier:Float = 1.0
	Field feedTime:Float = 1.8
	Field sampleText:String = "Normal-Dodge" 'half of this texts width will be translated to the left

'--------------------------------------------------------------------------
' * Init
'--------------------------------------------------------------------------	
	Method New()
		Super.New(0, 0, 0, 0)
	End
	
	Method InitWithSizeAndFont:Void(initSize:Int, fontName:String)
		usedFont = FontCache.GetFont(fontName)
		lineHeight = usedFont.height * 0.9
		maxItems = initSize
		items = New LabelFeedItem[maxItems]
		timeAlive = New Float[maxItems]
		For Local i:Int = 0 Until maxItems
			items[i] = New LabelFeedItem("")
			items[i].SetFont(usedFont)
		Next
	End

	Method SetCallback:Void(callback:ILabelFeedCallback)
		Self.callback = callback
	End
	
	
'--------------------------------------------------------------------------
' * Settings
'--------------------------------------------------------------------------	
	Method SetIcon:Void(imagePath:String)
		Local icon:Image = ImageCache.GetImage(imagePath)
		For Local i:Int = 0 Until maxItems
			items[i].SetIcon(icon)
		Next
	End
	
	Method SetAlignment:Void(horizontal:Int, vertical:Int)
		For Local i:Int = 0 Until maxItems
			items[i].alignHorizontal = horizontal
			items[i].alignVertical = vertical
		Next
	End
	
	Method SetColor:Void(color:Color)
		For Local i:Int = 0 Until maxItems
			items[i].color.Set(color)
		Next
	End
	
	
'--------------------------------------------------------------------------
' * Do
'--------------------------------------------------------------------------	
	Method Push:Void(itemText:String)
		tempLabels.AddLast(itemText)
	End
	
	Method Clear:Void()
		For Local i:Int = 0 Until maxItems
			items[i].Text = ""
			items[i].scale.y = 0.0
			items[i].color.Alpha = 0.0
			timeAlive[i] = 0.0
		Next
	End


'--------------------------------------------------------------------------
' * Update / Render
'--------------------------------------------------------------------------
	Method Update:Void(dt:Float)
		If Not tempLabels.IsEmpty()
			nextPush -= dt
			If nextPush <= 0.0
				nextPush = PUSH_MINIMUM_TIME
				Local item:= tempLabels.RemoveLast()
				InitRealPush(item)
			End
		End
		
		For Local i:Int = 0 Until maxItems
			timeAlive[i] += dt
			If timeAlive[i] > feedTime
				items[i].color.Alpha -= dt * 1/PUSH_MINIMUM_TIME
				items[i].scale.y -= dt * 1/PUSH_MINIMUM_TIME
			End
		Next
		
		Local item:= items[0]
		If timeAlive[0] < PUSH_MINIMUM_TIME
			item.color.Alpha += dt * 1/PUSH_MINIMUM_TIME
			item.scale.y = item.color.Alpha
			If timeAlive[0] + dt >= PUSH_MINIMUM_TIME
				item.scale.y = 1.0
				item.color.Alpha = 1.0
			End
		End
		
	End
	
	Method Render:Void()
		PushMatrix()
			Translate(position.x - usedFont.TextWidth(sampleText)/2, position.y)
			Local offsetY:Float = -lineHeight
			Local currentScore:Int = 0
			For Local i:Int = 0 Until maxItems
				If IsVisible(items[i])
					PushMatrix()
						Translate(0, offsetY + items[0].scale.y * lineHeight * lineHeightMultiplier)
						items[i].Render()
						offsetY += lineHeight
					PopMatrix()
				End
			Next
		PopMatrix()
	End
	
	Method IsVisible:Bool(item:LabelFeedItem)
		Return item.color.Alpha > 0.0 And item.scale.y > 0.0 And item.Text.Length > 0
	End

	
'--------------------------------------------------------------------------
' * Properties
'--------------------------------------------------------------------------
	Method Width:Float() Property
		Local w:Float = 0
		For Local i:Int = 0 Until maxItems
			If Not IsVisible(items[i])
				Exit
			End
			w = Max(w, items[i].size.x)
		Next
		Return w
	End
	
	Method Height:Float() Property
		Local h:Float = 0
		For Local i:Int = 0 Until maxItems
			If Not IsVisible(items[i])
				Exit
			End
			h = Max(h, items[i].size.y)
		Next
		Return h
	End
	
	Method IsFull:Bool() Property
		For Local i:Int = 0 Until maxItems
			If IsVisible(items[i]) = False
				Return False
			End
		Next
		Return True
	End
	
	Method ActiveItems:Int() Property
		Local count:Int
		For Local i:Int = 0 Until maxItems
			If IsVisible(items[i])
				count += 1
			End
		Next
		Return count
	End
	
	
'--------------------------------------------------------------------------
' * Private
'--------------------------------------------------------------------------
	Private
	Method InitRealPush:Void(itemText:String)
		Local lastItem:= items[maxItems-1]
		
		For Local i:Int = maxItems-1 To 1 Step -1
			items[i] = items[i-1]
			timeAlive[i] = timeAlive[i-1]
		Next
		
		items[0] = lastItem
		items[0].Text = itemText
		items[0].scale.y = 0.0
		items[0].color.Alpha = 0.0
		timeAlive[0] = 0.0
		
		If callback
			callback.OnLabelPush(items[0])
		End
	End
	
	Private
	Field maxItems:Int = 5
	Field items:LabelFeedItem[]	
	Field timeAlive:Float[]
	
	Field tempLabels:StringList = New StringList
	Field nextPush:Float = PUSH_MINIMUM_TIME
	Const PUSH_MINIMUM_TIME:Float = 0.2
	
	Field usedFont:AngelFont
	Field lineHeight:Float
	
	Field callback:ILabelFeedCallback
	
End



Class LabelFeedItem Extends VLabel
	
	Method New(text:String)
		Super.New(text)
		alignHorizontal = AngelFont.ALIGN_LEFT
		alignVertical = AngelFont.ALIGN_CENTER
	End
	
	Method SetFont:Void(font:AngelFont)
		Super.SetFont(font)
		UpdateIconPosition()
	End
	
	Method SetIcon:Void(icon:Image)
		Self.icon = icon
		UpdateIconPosition()
	End
	
	Method Text:Void(text:String) Property
		Super.Text(text)
		UpdateIconPosition()
	End
	
	Method Text:String() Property
		Return Super.Text
	End
	
	Method Draw:Void()
		If Self.Text
			#If TARGET = "html5"
				Color.White.UseWithoutAlpha()
			#End
			If icon
				DrawImage(icon, iconX, iconY)
			End
			Super.Draw()
		End
	End
	
	Private
	Field iconX:Float
	Field iconY:Float
	Field icon:Image
	
	Method UpdateIconPosition:Void()
		If icon
			iconX = -icon.Width() * 1.2
			iconY = -icon.Height() * 0.5 + 3
		End
	End
	
End









