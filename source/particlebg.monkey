Strict
Import vsat
Import particles
Import extra


Class ParticleBackground
	
	Field emitter:ParticleEmitter
	Field lastFrame:Int
	Field gradient:VSprite
	
	Method New()
		Local baseUnit:Float = Vsat.ScreenWidth2
		emitter = New ParticleEmitter
		emitter.InitWithSize(120)
		emitter.particleLifeSpan = 12
		emitter.particleLifeSpanVariance = 5.0
		emitter.emissionRate = 10
		
		emitter.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.75)
		emitter.positionVariance.Set(baseUnit, Vsat.ScreenHeight2)
		emitter.size.Set(1, 1)
		emitter.endSize.Set(24, 24)
		If IsHD()
			emitter.endSize.Mul(2)
		End

		emitter.startColor.Alpha = 0.1
		emitter.endColor.Alpha = 0.0
		emitter.emissionAngle = -90
		emitter.speed = 45
		emitter.speedVariance = 25
		If IsHD()
			emitter.speed *= 2
			emitter.speedVariance *= 2
		End
		
		emitter.Start()
		emitter.FastForward(20, 0.01666)
	End
	
	Method Update:Void(dt:Float)
		If lastFrame = Vsat.Frame
			Return
		End
		lastFrame = Vsat.Frame
		emitter.Update(dt)
	End
	
	Method Render:Void()
		emitter.Render()
	End
	
End
