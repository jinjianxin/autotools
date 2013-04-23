Name:		bluehfp
Version:	1.0
Release:	7_haoke
Summary:	Phone pdaemon

Group:		Applications/System
License:	GPL	
Source:	    %{name}-%{version}.tar.gz
BuildRoot:	/var/tmp/%{name}-root
#Requires:	clutter-1.0 gtk+-2.0 hildon-1

%description
phond pdaemon

%package devel
Summary:development files
Group:System Environment/Libraries
 
%description devel
libraries and header files

%prep
%setup -q

%build
#./autogen.sh --prefix=/usr
./configure --prefix=/usr
make 


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_libdir}/libbluehfp.so

%files devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/pkgconfig/libbluehfp-1.0.pc

%changelog
* Tue Nov 6 2012 jinjianxin <jinjianxinjin@gmail.com> 1.0.7
- 修复不能发出一些特殊字符的bug
* Wed Oct 31 2012 jinjianxin <jinjianxinjin@gmail.com> 1.0.6
- 增加对短信发送成功消息的处理
* Mon Oct 29 2012 jinjianxin <jinjianxinjin@gmail.com> 1.0.5
- 解决短信7位编码的问题
* Thu Oct 25 2012 jinjianxin <jinjianxinjin@gmail.com> 1.0.4
- 将短信加入排队处理，解决程序段错误问题，解决长短信的接收问题，解决短信发送时的返回值 增加获取信号强度的处理
* Wed Oct 24 2012 jinjianxin <jinjianxinjin@gmail.com> 1.0.3
- 解决audiomanager不能正常注册问题
* Tue Oct 23 2012 jinjianxin <jinjianxinjin@gmail.com> 1.0.2
- 解决机器重启不能打出电话问题，部分短信解析不正常问题
* Mon Oct 22 2012 jinjianxin<jinjianxinjin@gmail.com> 1.0.1
- init
